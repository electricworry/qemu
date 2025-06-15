/*
 * Orange Pi Zero 3 emulation
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
#include "qemu/units.h"
#include "system/address-spaces.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/arm/allwinner-h616.h"
#include "hw/arm/boot.h"

static struct arm_boot_info orangepi_binfo;

static void orangepi_init(MachineState *machine)
{
    printf("orangepi_init start\n");
    AwH616State *h616;
    DriveInfo *di;
    BlockBackend *blk;
    BusState *bus;
    DeviceState *carddev;

    /* BIOS is not supported by this board */
    if (machine->firmware) {
        error_report("BIOS not supported for this machine");
        exit(1);
    }

    /* This board has fixed RAM sizes*/
    if (machine->ram_size != 1 * GiB &&
        machine->ram_size != 2 * GiB &&
        machine->ram_size != 4 * GiB
       ) {
        error_report("This machine can only be used with 1/2/4 GiB of RAM");
        exit(1);
    }

    h616 = AW_H616(object_new(TYPE_AW_H616));
    object_property_add_child(OBJECT(machine), "soc", OBJECT(h616));
    object_unref(OBJECT(h616));

    /* Setup timer properties */ // TODO: How do I figure these out?
    object_property_set_int(OBJECT(h616), "clk0-freq", 32768, &error_abort);
    object_property_set_int(OBJECT(h616), "clk1-freq", 24 * 1000 * 1000,
                            &error_abort);

    /* Setup SID properties. Currently using a default fixed SID identifier. */ // TODO, check what these values should be.
    if (qemu_uuid_is_null(&h616->sid.identifier)) {
        qdev_prop_set_string(DEVICE(h616), "identifier",
                             "02c00081-1111-2222-3333-000044556677");
    } else if (ldl_be_p(&h616->sid.identifier.data[0]) != 0x02c00081) {
        warn_report("Security Identifier value does not include H616 prefix");
    }

    /* Setup EMAC properties */
    object_property_set_int(OBJECT(&h616->emac0), "phy-addr", 1, &error_abort);
    object_property_set_int(OBJECT(&h616->emac1), "phy-addr", 1, &error_abort);

    /* DRAMC */
    object_property_set_uint(OBJECT(h616), "ram-addr", h616->memmap[AW_H616_DEV_SDRAM],
                             &error_abort);
    object_property_set_int(OBJECT(h616), "ram-size", machine->ram_size / MiB,
                            &error_abort);

    /* Mark H616 object realized */
    qdev_realize(DEVICE(h616), NULL, &error_abort);

    /* Retrieve SD bus */
    di = drive_get(IF_SD, 0, 0);
    blk = di ? blk_by_legacy_dinfo(di) : NULL;
    bus = qdev_get_child_bus(DEVICE(h616), "sd-bus");

    /* Plug in SD card */
    carddev = qdev_new(TYPE_SD_CARD);
    qdev_prop_set_drive_err(carddev, "drive", blk, &error_fatal);
    qdev_realize_and_unref(carddev, bus, &error_fatal);

    /* SDRAM */
    memory_region_add_subregion(get_system_memory(), h616->memmap[AW_H616_DEV_SDRAM],
                                machine->ram);

    /* Load target kernel or start using BootROM */
    if (!machine->kernel_filename && blk && blk_is_available(blk)) {
        /* Use Boot ROM to copy data from SD card to SRAM */
        allwinner_h616_bootrom_setup(h616, blk);
    }
    orangepi_binfo.loader_start = h616->memmap[AW_H616_DEV_SDRAM];
    orangepi_binfo.ram_size = machine->ram_size;
    orangepi_binfo.psci_conduit = QEMU_PSCI_CONDUIT_SMC;
    arm_load_kernel(&h616->cpus[0], machine, &orangepi_binfo);
    
    printf("orangepi_init exit\n");
}

static void orangepi_machine_init(MachineClass *mc)
{
    static const char * const valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-a53"),
        NULL
    };

    mc->desc = "Orange Pi Zero 3 (Cortex-A53)";
    mc->init = orangepi_init;
    mc->block_default_type = IF_SD;
    mc->units_per_default_bus = 1;
    mc->min_cpus = AW_H616_NUM_CPUS;
    mc->max_cpus = AW_H616_NUM_CPUS;
    mc->default_cpus = AW_H616_NUM_CPUS;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a53");
    mc->valid_cpu_types = valid_cpu_types;
    mc->default_ram_size = 4 * GiB;
    mc->default_ram_id = "orangepi-zero3.ram";
    mc->auto_create_sdcard = true;
}

DEFINE_MACHINE("orangepi-zero3", orangepi_machine_init)
