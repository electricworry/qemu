/*
 * Allwinner H616 System on Chip emulation
 *
 * Copyright (C) 2019 Niek Linnenbank <nieklinnenbank@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "qemu/units.h"
#include "hw/qdev-core.h"
#include "hw/sysbus.h"
#include "hw/char/serial-mm.h"
#include "hw/misc/unimp.h"
#include "hw/usb/hcd-ehci.h"
#include "hw/loader.h"
#include "system/system.h"
#include "hw/arm/allwinner-h616.h"
#include "target/arm/cpu-qom.h"
#include "target/arm/gtimer.h"

/* Memory map */
const hwaddr allwinner_h616_memmap[] = {
    // [AW_H616_DEV_SRAM_A1]    = 0x00020000,
    // [AW_H616_DEV_SRAM_C]     = 0x00028000,

    // [AW_H616_DEV_SYSCTRL]    = 0x01c00000,
    // [AW_H616_DEV_MMC0]       = 0x01c0f000,
    // [AW_H616_DEV_SID]        = 0x01c14000,
    // [AW_H616_DEV_EHCI0]      = 0x01c1a000,
    // [AW_H616_DEV_OHCI0]      = 0x01c1a400,
    // [AW_H616_DEV_EHCI1]      = 0x01c1b000,
    // [AW_H616_DEV_OHCI1]      = 0x01c1b400,
    // [AW_H616_DEV_EHCI2]      = 0x01c1c000,
    // [AW_H616_DEV_OHCI2]      = 0x01c1c400,
    // [AW_H616_DEV_EHCI3]      = 0x01c1d000,
    // [AW_H616_DEV_OHCI3]      = 0x01c1d400,
    // [AW_H616_DEV_CCU]        = 0x01c20000,
    // [AW_H616_DEV_PIT]        = 0x01c20c00,
    // [AW_H616_DEV_WDT]        = 0x01c20ca0,
    [AW_H616_DEV_UART0]      = 0x05000000,
    // [AW_H616_DEV_UART1]      = 0x01c28400,
    // [AW_H616_DEV_UART2]      = 0x01c28800,
    // [AW_H616_DEV_UART3]      = 0x01c28c00,
    // [AW_H616_DEV_TWI0]       = 0x01c2ac00,
    // [AW_H616_DEV_TWI1]       = 0x01c2b000,
    // [AW_H616_DEV_TWI2]       = 0x01c2b400,
    // [AW_H616_DEV_EMAC]       = 0x01c30000,
    // [AW_H616_DEV_DRAMCOM]    = 0x01c62000,
    // [AW_H616_DEV_DRAMCTL]    = 0x01c63000,
    // [AW_H616_DEV_DRAMPHY]    = 0x01c65000,
    [AW_H616_DEV_GIC_DIST]   = 0x03020000, // 0x01c81000
    // [AW_H616_DEV_GIC_CPU]    = 0x03022000,
    // [AW_H616_DEV_GIC_HYP]    = 0x03024000,
    // [AW_H616_DEV_GIC_VCPU]   = 0x03026000,
    // [AW_H616_DEV_RTC]        = 0x01f00000,
    // [AW_H616_DEV_CPUCFG]     = 0x01f01c00,
    // [AW_H616_DEV_R_TWI]      = 0x01f02400,
    [AW_H616_DEV_SDRAM]      = 0x40000000
};

/* List of unimplemented devices */
static struct AwH616Unimplemented {
    const char *device_name;
    hwaddr base;
    hwaddr size;
} unimplemented[] = {
    { "brom",           0x00000000, 64 * KiB },
    { "sram-a1",        0x00020000, 32 * KiB },
    { "sram-c",         0x00028000, (128+64) * KiB },
    // Accelerator
    { "d-engine",       0x01000000, 4 * MiB },
    { "dio",            0x01420000, 256 * KiB },
    { "g2d",            0x01480000, 256 * KiB },
    { "gpu",            0x01800000, 256 * KiB },
    { "ce-ns",          0x01904000, 2 * KiB },
    { "ce-s",           0x01904800, 2 * KiB },
    { "ce-key-sram",    0x01908000, 4 * KiB },
    { "ve-sram",        0x01a00000, 2 * MiB },
    { "ve",             0x01c0e000, 8 * KiB },
    // System Resources
    { "sys-cfg",        0x03000000, 4 * KiB },
    { "ccu",            0x03001000, 4 * KiB },
    { "dma",            0x03002000, 4 * KiB },
    { "hstimer",        0x03005000, 4 * KiB },
    { "sid",            0x03006000, 4 * KiB },
    { "smc",            0x03007000, 4 * KiB },
    { "spc",            0x03008000, 1 * KiB },
    { "timer",          0x03009000, 1 * KiB },
    { "pwm",            0x0300a000, 1 * KiB },
    { "gpio",           0x0300b000, 1 * KiB },
    { "psi",            0x0300c000, 1 * KiB },
    { "gic",            0x03020000, 64 * KiB },
    { "iommu",          0x030f0000, 64 * KiB },
    { "rtc",            0x07000000, 1 * KiB },
    { "prcm",           0x07010000, 1 * KiB }, // Power Reset Clock Management
    { "twd",            0x07020800, 1 * KiB },
    // Memory
    { "nand0",          0x04011000, 4 * KiB },
    { "smhc0",          0x04020000, 4 * KiB },
    { "smhc1",          0x04021000, 4 * KiB },
    { "smhc2",          0x04022000, 4 * KiB },
    { "msi-ctrl",       0x047fa000, 4 * KiB },
    { "dram-ctrl",      0x047fb000, 20 * KiB },
    { "phy-ctrl",       0x04800000, 8 * MiB },
    // Interfaces
    { "uart1",          0x05000400, 1 * KiB },
    { "uart2",          0x05000800, 1 * KiB },
    { "uart3",          0x05000c00, 1 * KiB },
    { "uart4",          0x05001000, 1 * KiB },
    { "uart5",          0x05001400, 1 * KiB },
    { "twi0",           0x05002000, 1 * KiB },
    { "twi1",           0x05002400, 1 * KiB },
    { "twi2",           0x05002800, 1 * KiB },
    { "twi3",           0x05002c00, 1 * KiB },
    { "twi4",           0x05003000, 1 * KiB },
    { "s-twi0",         0x07081400, 1 * KiB },
    { "spi0",           0x05010000, 4 * KiB },
    { "spi1",           0x05011000, 4 * KiB },
    { "emac0",          0x05020000, 64 * KiB },
    { "emac1",          0x05030000, 64 * KiB },
    { "ts0",            0x05060000, 4 * KiB },
    { "ths",            0x05070400, 1 * KiB },
    { "lradc",          0x05070800, 1 * KiB },
    { "owa",            0x05093000, 1 * KiB },
    { "dmic",           0x05095000, 1 * KiB },
    { "audio-codec",    0x05096000, 4 * KiB },
    { "audio-hub",      0x05097000, 4 * KiB },
    { "usb0-otg",       0x05100000, 1 * MiB },
    { "usb1",           0x05200000, 1 * MiB },
    { "usb2",           0x05310000, 4 * KiB },
    { "usb3",           0x05311000, 4 * KiB },
    { "cir_rx",         0x07040000, 1 * KiB },
    // Display
    { "hdmi-tx0",       0x06000000, 1 * MiB },
    { "disp-if-top",    0x06510000, 4 * KiB },
    { "tcon-tv0",       0x06515000, 4 * KiB },
    { "tcon-tv1",       0x06516000, 4 * KiB },
    { "tve-top",        0x06520000, 16 * KiB },
    { "tve0",           0x06524000, 16 * KiB },
    // CPUX Related
    { "cpu-subsys-cfg", 0x08100000, 1 * KiB },
    { "timestamp-stu",  0x08110000, 4 * KiB },
    { "timestamp-ctrl", 0x08120000, 4 * KiB },
    { "idc",            0x08130000, 3 * KiB },
    { "c0-cpux-cfg",    0x09010000, 1 * KiB },
    { "c0-cpux-mbist",  0x09020000, 4 * KiB },
};

/* Per Processor Interrupts */
enum {
    AW_H616_GIC_PPI_MAINT     =  9,
    AW_H616_GIC_PPI_HYPTIMER  = 10,
    AW_H616_GIC_PPI_VIRTTIMER = 11,
    AW_H616_GIC_PPI_SECTIMER  = 13,
    AW_H616_GIC_PPI_PHYSTIMER = 14
};

/* Shared Processor Interrupts User manual page 159 (After SGI/PPI list)*/
enum {
    AW_H616_GIC_SPI_UART0     =  0,
    AW_H616_GIC_SPI_UART1     =  1,
    AW_H616_GIC_SPI_UART2     =  2,
    AW_H616_GIC_SPI_UART3     =  3,
    AW_H616_GIC_SPI_UART4     =  4,
    AW_H616_GIC_SPI_UART5     =  5,
    AW_H616_GIC_SPI_TWI0      =  6,
    AW_H616_GIC_SPI_TWI1      =  7,
    AW_H616_GIC_SPI_TWI2      =  8,
    AW_H616_GIC_SPI_TWI3      =  9,
    AW_H616_GIC_SPI_TWI4      = 10,
    AW_H616_GIC_SPI_TIMER0    = 48,
    AW_H616_GIC_SPI_TIMER1    = 49,
    AW_H616_GIC_SPI_R_TWI     = 44,
    AW_H616_GIC_SPI_MMC0      = 60,
    AW_H616_GIC_SPI_EHCI0     = 72,
    AW_H616_GIC_SPI_OHCI0     = 73,
    AW_H616_GIC_SPI_EHCI1     = 74,
    AW_H616_GIC_SPI_OHCI1     = 75,
    AW_H616_GIC_SPI_EHCI2     = 76,
    AW_H616_GIC_SPI_OHCI2     = 77,
    AW_H616_GIC_SPI_EHCI3     = 78,
    AW_H616_GIC_SPI_OHCI3     = 79,
    AW_H616_GIC_SPI_EMAC      = 82
};

/* Allwinner H616 general constants */
enum {
    AW_H616_GIC_NUM_SPI       = 128
};

void allwinner_h616_bootrom_setup(AwH616State *s, BlockBackend *blk)
{
    const int64_t rom_size = 32 * KiB;
    g_autofree uint8_t *buffer = g_new0(uint8_t, rom_size);

    if (blk_pread(blk, 8 * KiB, rom_size, buffer, 0) < 0) {
        error_report("%s: failed to read BlockBackend data", __func__);
        exit(1);
    }

    rom_add_blob("allwinner-h616.bootrom", buffer, rom_size,
                  rom_size, s->memmap[AW_H616_DEV_SRAM_A1],
                  NULL, NULL, NULL, NULL, false);
}

static void allwinner_h616_init(Object *obj)
{
    AwH616State *s = AW_H616(obj);

    s->memmap = allwinner_h616_memmap;

    for (int i = 0; i < AW_H616_NUM_CPUS; i++) {
        object_initialize_child(obj, "cpu[*]", &s->cpus[i],
                                ARM_CPU_TYPE_NAME("cortex-a53"));
    }

    object_initialize_child(obj, "gic", &s->gic, TYPE_ARM_GICV3);

    object_initialize_child(obj, "timer", &s->timer, TYPE_AW_A10_PIT);
    object_property_add_alias(obj, "clk0-freq", OBJECT(&s->timer),
                              "clk0-freq");
    object_property_add_alias(obj, "clk1-freq", OBJECT(&s->timer),
                              "clk1-freq");

    object_initialize_child(obj, "ccu", &s->ccu, TYPE_AW_H616_CCU);

    object_initialize_child(obj, "sysctrl", &s->sysctrl, TYPE_AW_H616_SYSCTRL);

    object_initialize_child(obj, "cpucfg", &s->cpucfg, TYPE_AW_CPUCFG);

    object_initialize_child(obj, "sid", &s->sid, TYPE_AW_SID);
    object_property_add_alias(obj, "identifier", OBJECT(&s->sid),
                              "identifier");

    object_initialize_child(obj, "mmc0", &s->mmc0, TYPE_AW_SDHOST_SUN5I);

    object_initialize_child(obj, "emac", &s->emac, TYPE_AW_SUN8I_EMAC);

    object_initialize_child(obj, "dramc", &s->dramc, TYPE_AW_H616_DRAMC);
    object_property_add_alias(obj, "ram-addr", OBJECT(&s->dramc),
                             "ram-addr");
    object_property_add_alias(obj, "ram-size", OBJECT(&s->dramc),
                              "ram-size");

    object_initialize_child(obj, "rtc", &s->rtc, TYPE_AW_RTC_SUN6I);

    object_initialize_child(obj, "twi0",  &s->i2c0,  TYPE_AW_I2C_SUN6I);
    object_initialize_child(obj, "twi1",  &s->i2c1,  TYPE_AW_I2C_SUN6I);
    object_initialize_child(obj, "twi2",  &s->i2c2,  TYPE_AW_I2C_SUN6I);
    object_initialize_child(obj, "r_twi", &s->r_twi, TYPE_AW_I2C_SUN6I);

    object_initialize_child(obj, "wdt", &s->wdt, TYPE_AW_WDT_SUN6I);
}

static void allwinner_h616_realize(DeviceState *dev, Error **errp)
{
    AwH616State *s = AW_H616(dev);
    unsigned i;

    /* CPUs */
    for (i = 0; i < AW_H616_NUM_CPUS; i++) {

        /*
         * Disable secondary CPUs. Guest EL3 firmware will start
         * them via CPU reset control registers.
         */
        qdev_prop_set_bit(DEVICE(&s->cpus[i]), "start-powered-off",
                          i > 0);

        /* All exception levels required */
        qdev_prop_set_bit(DEVICE(&s->cpus[i]), "has_el3", true);
        qdev_prop_set_bit(DEVICE(&s->cpus[i]), "has_el2", true);

        /* Mark realized */
        qdev_realize(DEVICE(&s->cpus[i]), NULL, &error_fatal);
    }

    /* Generic Interrupt Controller */
    qdev_prop_set_uint32(DEVICE(&s->gic), "num-irq", AW_H616_GIC_NUM_SPI +
                                                     GIC_INTERNAL);
    qdev_prop_set_uint32(DEVICE(&s->gic), "revision", 2);
    qdev_prop_set_uint32(DEVICE(&s->gic), "num-cpu", AW_H616_NUM_CPUS);
    qdev_prop_set_bit(DEVICE(&s->gic), "has-security-extensions", false);
    qdev_prop_set_bit(DEVICE(&s->gic), "has-virtualization-extensions", true);
    sysbus_realize(SYS_BUS_DEVICE(&s->gic), &error_fatal);

    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gic), 0, s->memmap[AW_H616_DEV_GIC_DIST]);
    // sysbus_mmio_map(SYS_BUS_DEVICE(&s->gic), 1, s->memmap[AW_H616_DEV_GIC_CPU]);
    // sysbus_mmio_map(SYS_BUS_DEVICE(&s->gic), 2, s->memmap[AW_H616_DEV_GIC_HYP]);
    // sysbus_mmio_map(SYS_BUS_DEVICE(&s->gic), 3, s->memmap[AW_H616_DEV_GIC_VCPU]);

    /*
     * Wire the outputs from each CPU's generic timer and the GICv3
     * maintenance interrupt signal to the appropriate GIC PPI inputs,
     * and the GIC's IRQ/FIQ/VIRQ/VFIQ interrupt outputs to the CPU's inputs.
     */
    for (i = 0; i < AW_H616_NUM_CPUS; i++) {
        DeviceState *cpudev = DEVICE(&s->cpus[i]);
        int ppibase = AW_H616_GIC_NUM_SPI + i * GIC_INTERNAL + GIC_NR_SGIS;
        int irq;
        /*
         * Mapping from the output timer irq lines from the CPU to the
         * GIC PPI inputs used for this board.
         */
        const int timer_irq[] = {
            [GTIMER_PHYS] = AW_H616_GIC_PPI_PHYSTIMER,
            [GTIMER_VIRT] = AW_H616_GIC_PPI_VIRTTIMER,
            [GTIMER_HYP]  = AW_H616_GIC_PPI_HYPTIMER,
            [GTIMER_SEC]  = AW_H616_GIC_PPI_SECTIMER,
        };

        /* Connect CPU timer outputs to GIC PPI inputs */
        for (irq = 0; irq < ARRAY_SIZE(timer_irq); irq++) {
            qdev_connect_gpio_out(cpudev, irq,
                                  qdev_get_gpio_in(DEVICE(&s->gic),
                                                   ppibase + timer_irq[irq]));
        }

        /* Connect GIC outputs to CPU interrupt inputs */
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gic), i,
                           qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gic), i + AW_H616_NUM_CPUS,
                           qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gic), i + (2 * AW_H616_NUM_CPUS),
                           qdev_get_gpio_in(cpudev, ARM_CPU_VIRQ));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gic), i + (3 * AW_H616_NUM_CPUS),
                           qdev_get_gpio_in(cpudev, ARM_CPU_VFIQ));

        /* GIC maintenance signal */
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gic), i + (4 * AW_H616_NUM_CPUS),
                           qdev_get_gpio_in(DEVICE(&s->gic),
                                            ppibase + AW_H616_GIC_PPI_MAINT));
    }

    /* Timer */
    sysbus_realize(SYS_BUS_DEVICE(&s->timer), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->timer), 0, s->memmap[AW_H616_DEV_PIT]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->timer), 0,
                       qdev_get_gpio_in(DEVICE(&s->gic), AW_H616_GIC_SPI_TIMER0));
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->timer), 1,
                       qdev_get_gpio_in(DEVICE(&s->gic), AW_H616_GIC_SPI_TIMER1));

    /* SRAM */
    memory_region_init_ram(&s->sram_a1, OBJECT(dev), "sram A1",
                            64 * KiB, &error_abort);
    memory_region_init_ram(&s->sram_a2, OBJECT(dev), "sram A2",
                            32 * KiB, &error_abort);
    memory_region_init_ram(&s->sram_c, OBJECT(dev), "sram C",
                            44 * KiB, &error_abort);
    memory_region_add_subregion(get_system_memory(), s->memmap[AW_H616_DEV_SRAM_A1],
                                &s->sram_a1);
    memory_region_add_subregion(get_system_memory(), s->memmap[AW_H616_DEV_SRAM_A2],
                                &s->sram_a2);
    memory_region_add_subregion(get_system_memory(), s->memmap[AW_H616_DEV_SRAM_C],
                                &s->sram_c);

    /* Clock Control Unit */
    sysbus_realize(SYS_BUS_DEVICE(&s->ccu), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->ccu), 0, s->memmap[AW_H616_DEV_CCU]);

    /* System Control */
    sysbus_realize(SYS_BUS_DEVICE(&s->sysctrl), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->sysctrl), 0, s->memmap[AW_H616_DEV_SYSCTRL]);

    /* CPU Configuration */
    sysbus_realize(SYS_BUS_DEVICE(&s->cpucfg), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->cpucfg), 0, s->memmap[AW_H616_DEV_CPUCFG]);

    /* Security Identifier */
    sysbus_realize(SYS_BUS_DEVICE(&s->sid), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->sid), 0, s->memmap[AW_H616_DEV_SID]);

    /* SD/MMC */
    object_property_set_link(OBJECT(&s->mmc0), "dma-memory",
                             OBJECT(get_system_memory()), &error_fatal);
    sysbus_realize(SYS_BUS_DEVICE(&s->mmc0), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->mmc0), 0, s->memmap[AW_H616_DEV_MMC0]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->mmc0), 0,
                       qdev_get_gpio_in(DEVICE(&s->gic), AW_H616_GIC_SPI_MMC0));

    object_property_add_alias(OBJECT(s), "sd-bus", OBJECT(&s->mmc0),
                              "sd-bus");

    /* EMAC */
    qemu_configure_nic_device(DEVICE(&s->emac), true, NULL);
    object_property_set_link(OBJECT(&s->emac), "dma-memory",
                             OBJECT(get_system_memory()), &error_fatal);
    sysbus_realize(SYS_BUS_DEVICE(&s->emac), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->emac), 0, s->memmap[AW_H616_DEV_EMAC]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->emac), 0,
                       qdev_get_gpio_in(DEVICE(&s->gic), AW_H616_GIC_SPI_EMAC));

    /* Universal Serial Bus */
    sysbus_create_simple(TYPE_AW_H3_EHCI, s->memmap[AW_H616_DEV_EHCI0],
                         qdev_get_gpio_in(DEVICE(&s->gic),
                                          AW_H616_GIC_SPI_EHCI0));
    sysbus_create_simple(TYPE_AW_H3_EHCI, s->memmap[AW_H616_DEV_EHCI1],
                         qdev_get_gpio_in(DEVICE(&s->gic),
                                          AW_H616_GIC_SPI_EHCI1));
    sysbus_create_simple(TYPE_AW_H3_EHCI, s->memmap[AW_H616_DEV_EHCI2],
                         qdev_get_gpio_in(DEVICE(&s->gic),
                                          AW_H616_GIC_SPI_EHCI2));
    sysbus_create_simple(TYPE_AW_H3_EHCI, s->memmap[AW_H616_DEV_EHCI3],
                         qdev_get_gpio_in(DEVICE(&s->gic),
                                          AW_H616_GIC_SPI_EHCI3));

    sysbus_create_simple("sysbus-ohci", s->memmap[AW_H616_DEV_OHCI0],
                         qdev_get_gpio_in(DEVICE(&s->gic),
                                          AW_H616_GIC_SPI_OHCI0));
    sysbus_create_simple("sysbus-ohci", s->memmap[AW_H616_DEV_OHCI1],
                         qdev_get_gpio_in(DEVICE(&s->gic),
                                          AW_H616_GIC_SPI_OHCI1));
    sysbus_create_simple("sysbus-ohci", s->memmap[AW_H616_DEV_OHCI2],
                         qdev_get_gpio_in(DEVICE(&s->gic),
                                          AW_H616_GIC_SPI_OHCI2));
    sysbus_create_simple("sysbus-ohci", s->memmap[AW_H616_DEV_OHCI3],
                         qdev_get_gpio_in(DEVICE(&s->gic),
                                          AW_H616_GIC_SPI_OHCI3));

    /* UART0. For future clocktree API: All UARTS are connected to APB2_CLK. */
    serial_mm_init(get_system_memory(), s->memmap[AW_H616_DEV_UART0], 2,
                   qdev_get_gpio_in(DEVICE(&s->gic), AW_H616_GIC_SPI_UART0),
                   115200, serial_hd(0), DEVICE_LITTLE_ENDIAN);
    /* UART1 */
    serial_mm_init(get_system_memory(), s->memmap[AW_H616_DEV_UART1], 2,
                   qdev_get_gpio_in(DEVICE(&s->gic), AW_H616_GIC_SPI_UART1),
                   115200, serial_hd(1), DEVICE_LITTLE_ENDIAN);
    /* UART2 */
    serial_mm_init(get_system_memory(), s->memmap[AW_H616_DEV_UART2], 2,
                   qdev_get_gpio_in(DEVICE(&s->gic), AW_H616_GIC_SPI_UART2),
                   115200, serial_hd(2), DEVICE_LITTLE_ENDIAN);
    /* UART3 */
    serial_mm_init(get_system_memory(), s->memmap[AW_H616_DEV_UART3], 2,
                   qdev_get_gpio_in(DEVICE(&s->gic), AW_H616_GIC_SPI_UART3),
                   115200, serial_hd(3), DEVICE_LITTLE_ENDIAN);

    /* DRAMC */
    sysbus_realize(SYS_BUS_DEVICE(&s->dramc), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->dramc), 0, s->memmap[AW_H616_DEV_DRAMCOM]);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->dramc), 1, s->memmap[AW_H616_DEV_DRAMCTL]);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->dramc), 2, s->memmap[AW_H616_DEV_DRAMPHY]);

    /* RTC */
    sysbus_realize(SYS_BUS_DEVICE(&s->rtc), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->rtc), 0, s->memmap[AW_H616_DEV_RTC]);

    /* I2C */
    sysbus_realize(SYS_BUS_DEVICE(&s->i2c0), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->i2c0), 0, s->memmap[AW_H616_DEV_TWI0]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->i2c0), 0,
                       qdev_get_gpio_in(DEVICE(&s->gic), AW_H616_GIC_SPI_TWI0));

    sysbus_realize(SYS_BUS_DEVICE(&s->i2c1), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->i2c1), 0, s->memmap[AW_H616_DEV_TWI1]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->i2c1), 0,
                       qdev_get_gpio_in(DEVICE(&s->gic), AW_H616_GIC_SPI_TWI1));

    sysbus_realize(SYS_BUS_DEVICE(&s->i2c2), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->i2c2), 0, s->memmap[AW_H616_DEV_TWI2]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->i2c2), 0,
                       qdev_get_gpio_in(DEVICE(&s->gic), AW_H616_GIC_SPI_TWI2));

    sysbus_realize(SYS_BUS_DEVICE(&s->r_twi), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->r_twi), 0, s->memmap[AW_H616_DEV_R_TWI]);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->r_twi), 0,
                       qdev_get_gpio_in(DEVICE(&s->gic), AW_H616_GIC_SPI_R_TWI));

    /* WDT */
    sysbus_realize(SYS_BUS_DEVICE(&s->wdt), &error_fatal);
    sysbus_mmio_map_overlap(SYS_BUS_DEVICE(&s->wdt), 0,
                            s->memmap[AW_H616_DEV_WDT], 1);

    /* Unimplemented devices */
    for (i = 0; i < ARRAY_SIZE(unimplemented); i++) {
        create_unimplemented_device(unimplemented[i].device_name,
                                    unimplemented[i].base,
                                    unimplemented[i].size);
    }
}

static void allwinner_h616_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = allwinner_h616_realize;
    /* Reason: uses serial_hd() in realize function */
    dc->user_creatable = false;
}

static const TypeInfo allwinner_h616_type_info = {
    .name = TYPE_AW_H616,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(AwH616State),
    .instance_init = allwinner_h616_init,
    .class_init = allwinner_h616_class_init,
};

static void allwinner_h616_register_types(void)
{
    type_register_static(&allwinner_h616_type_info);
}

type_init(allwinner_h616_register_types)
