/*
 * Allwinner H616 Clock Control Unit emulation
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
#include "hw/sysbus.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/display/allwinner-h616-dw-hdmi.h"
#include "hw/irq.h"

/* CCU register offsets */
enum {
    REG_DESIGN_ID       = 0x0000, /* ??? */
    REG_REVISION_ID           = 0x0001, /* ??? */
    REG_PROD_ID_0           = 0x0002, /* ??? */
    REG_PROD_ID_1           = 0x0003, /* ??? */
    REG_PHY_TYPE           = 0x0006,
    REG_HDMI_PHY_STAT0     = 0x3004,
    REG_HDMI_IH_I2CM_STAT0 = 0x0105, // Interrupt register
    REG_HDMI_IH_MUTE_I2CM_STAT0 = 0x0185, // mute
    REG_HDMI_I2CM_SLAVE    = 0x7E00, // Driver sets the I2C address
    REG_HDMI_I2CM_ADDRESS  = 0x7E01, // Offset of the EEPROM to read from
    REG_HDMI_I2CM_DATAO   = 0x7e02, // From driver to device
    REG_HDMI_I2CM_DATAI   = 0x7e03, // From device to driver
    REG_HDMI_I2CM_OPERATION =                    0x7E04, // Driver sets the operation.
};

// #define REG_INDEX(offset)    (offset / sizeof(uint32_t))
#define REG_INDEX(offset)    (offset)

/* CCU register flags */
enum {
    REG_DRAM_CFG_UPDATE      = (1 << 16),
};

enum {
    REG_PLL_ENABLE           = (1 << 31),
    REG_PLL_LOCK             = (1 << 28),
};


/* CCU register reset values */
enum {
    REG_DESIGN_ID_RST           = 0x21, /* PLL CPUX Control */
    REG_REVISION_ID_RST           = 0x2a, /* PLL CPUX Control */
    REG_PROD_ID_0_RST           = 0xA0, /* Must be A0 */
    REG_PROD_ID_1_RST           = 0xC1, /* Must be 01 (2 MSB can be anything) */
    REG_PHY_TYPE_RST   = 0xF3, /* DW_HDMI_PHY_DWC_HDMI20_TX_PHY */
};

static void allwinner_h616_hdmi_reset(DeviceState *dev)
{
    AwH616HdmiState *s = AW_H616_HDMI(dev);

    /* Set default values for registers */
    s->regs[REG_INDEX(REG_DESIGN_ID)]           = REG_DESIGN_ID_RST;
    s->regs[REG_INDEX(REG_REVISION_ID)]           = REG_REVISION_ID_RST;
    s->regs[REG_INDEX(REG_PROD_ID_0)]           = REG_PROD_ID_0_RST;
    s->regs[REG_INDEX(REG_PROD_ID_1)]           = REG_PROD_ID_1_RST;
    s->regs[REG_INDEX(REG_PHY_TYPE)]           = REG_PHY_TYPE_RST;
    s->regs[0x3004] = 3; // [drm] Cannot find any crtc or sizes
}

static uint8_t edid_array[] = {
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x41, 0x0c, 0xe1, 0x08, 0x11, 0x04, 0x00, 0x00,
    0x19, 0x19, 0x01, 0x03, 0x80, 0x58, 0x31, 0x78, 0x2a, 0xf6, 0x3d, 0xa3, 0x55, 0x4e, 0x9e, 0x27,
    0x0d, 0x47, 0x4a, 0xbd, 0x4b, 0x00, 0xd1, 0xc0, 0x81, 0x80, 0x81, 0x40, 0x95, 0x0f, 0x95, 0x00,
    0xb3, 0x00, 0x81, 0xc0, 0x01, 0x01, 0x04, 0x74, 0x00, 0x30, 0xf2, 0x70, 0x5a, 0x80, 0xb0, 0x58,
    0x8a, 0x00, 0x6e, 0xe5, 0x31, 0x00, 0x00, 0x1a, 0x56, 0x5e, 0x00, 0xa0, 0xa0, 0xa0, 0x29, 0x50,
    0x30, 0x20, 0x35, 0x00, 0x6e, 0xe5, 0x31, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x50,
    0x48, 0x4c, 0x20, 0x42, 0x44, 0x4d, 0x34, 0x30, 0x36, 0x35, 0x0a, 0x20, 0x00, 0x00, 0x00, 0xfd,
    0x00, 0x17, 0x50, 0x1e, 0x63, 0x1e, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x92,

    0x02, 0x03, 0x2a, 0xf1, 0x4f, 0x01, 0x02, 0x03, 0x05, 0x06, 0x07, 0x10, 0x11, 0x12, 0x13, 0x14,
    0x15, 0x16, 0x1f, 0x04, 0x23, 0x09, 0x07, 0x07, 0x83, 0x01, 0x00, 0x00, 0x6d, 0x03, 0x0c, 0x00,
    0x20, 0x00, 0x00, 0x3c, 0x20, 0x00, 0x60, 0x01, 0x02, 0x03, 0x02, 0x3a, 0x80, 0xd0, 0x72, 0x38,
    0x2d, 0x40, 0x10, 0x2c, 0x96, 0x80, 0x6e, 0xe5, 0x31, 0x00, 0x00, 0x18, 0xef, 0x51, 0x00, 0xa0,
    0xf0, 0x70, 0x19, 0x80, 0x30, 0x20, 0x35, 0x00, 0x6e, 0xe5, 0x31, 0x00, 0x00, 0x1a, 0x04, 0x74,
    0x00, 0x30, 0xf2, 0x70, 0x5a, 0x80, 0xb0, 0x58, 0x8a, 0x00, 0x6e, 0xe5, 0x31, 0x00, 0x00, 0x1a,
    0x7d, 0x39, 0x00, 0xa0, 0x80, 0x38, 0x1f, 0x40, 0x30, 0x20, 0x3a, 0x00, 0x6e, 0xe5, 0x31, 0x00,
    0x00, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f,
};

static uint64_t allwinner_h616_hdmi_read(void *opaque, hwaddr offset,
                                      unsigned size)
{
    // printf("HDMIREAD: addr 0x%x size %d\n", (unsigned int) offset, size);

    uint8_t *ptr = (uint8_t *) &s->regs[offset];
    uint32_t result = 0;
    
    if (size == 1)
    {
        switch (offset) {
        case REG_HDMI_I2CM_DATAI:
            uint8_t read_offset = *(uint8_t *) &s->regs[REG_HDMI_I2CM_ADDRESS];
            printf("I2C_READ: offset 0x%x val: 0x%x\n", read_offset, edid_array[read_offset]);
            result = edid_array[read_offset];
            break;
        default:
            result = *((uint8_t *) ptr);
        }
    }
    else if (size == 4)
    {
        result = *((uint32_t *) ptr);
    }

    if (offset != 0x104 && offset != 0x105)
    {
        printf("HDMIREAD: addr 0x%x size %d val 0x%x\n", (unsigned int) offset, size, result);
    }
    return result;

    // const uint32_t idx = REG_INDEX(offset);

    // switch (offset) {
    // case 0x00004 ... AW_H616_HDMI_IOSIZE:
    //     qemu_log_mask(LOG_GUEST_ERROR, "%s: out-of-bounds offset 0x%04x\n",
    //                   __func__, (uint32_t)offset);
    //     return 0;
    // }

    // printf("HDMIREAD: addr 0x%x size %d val 0x%x\n", (unsigned int) offset, size, s->regs[idx]);
    // return s->regs[idx];
}

static void allwinner_h616_hdmi_update_irq(AwH616HdmiState *s)
{
    uint8_t mute = *((uint8_t *)&s->regs[0x815]);

    if (mute & HDMI_IH_I2CM_STAT0_DONE) // We're only going to return DONE
    {
        // Do nothing
    }
    else
    {
        printf("SEND IRQ!\n");
        uint32_t irq = 1;

        // if (s->global_ctl & SD_GCTL_INT_ENB) {
        //     irq = s->irq_status & s->irq_mask;
        // } else {
        //     irq = 0;
        // }

        // trace_allwinner_sdhost_update_irq(irq);
        qemu_set_irq(s->irq, irq);
    }
}

static void allwinner_h616_hdmi_write(void *opaque, hwaddr offset,
                                   uint64_t val, unsigned size)
{
    AwH616HdmiState *s = AW_H616_HDMI(opaque);
    printf("HDMIWRITE: addr 0x%x size %d val 0x%lx\n", (unsigned int) offset, size, val);
    uint8_t *ptr = (uint8_t *) &s->regs[offset];

    if (size == 1)
    {
        if (offset == 0x105 || offset == 0x104) // THIS CLEARS the stat
        {
            *ptr = *ptr & ~val;
            return;
        }
        *ptr = (uint8_t) val;
        if (offset == 0x3000)
        {
            if (!(val & 0x8))
            {
                printf("OFF\n");
                ptr = (uint8_t *) &s->regs[0x3004];
                *ptr = *ptr & 0xfe;
            }
            else
            {
                printf("ON\n");
                ptr = (uint8_t *) &s->regs[0x3004];
                *ptr = *ptr | 0x01;
            }
        }
        else if (offset == 0x185) // NOTHING. THIS JUST SETS THE MUTE MASK
        {
        }
        else if (offset == 0x7e04 && ((uint8_t)val) == HDMI_I2CM_OPERATION_READ) // HDMI_I2CM_OPERATION READ
        {
            *(uint8_t *)&s->regs[0x105] = HDMI_IH_I2CM_STAT0_DONE;
            allwinner_h616_hdmi_update_irq(s);
        }
    }
    else if (size == 4)
    {
        *((uint32_t *)ptr) = (uint32_t) val;
    }


    // const uint32_t idx = REG_INDEX(offset);

    // switch (offset) {
    // case REG_PLL_ENABLE:
    //     break;
    // default 
    // // // case REG_DRAM_CFG:    /* DRAM Configuration */
    // // //     val &= ~REG_DRAM_CFG_UPDATE;
    // // //     break;
    // // case REG_PLL_CPUX_CTRL:    /* PLL CPUX Control */
    // //     if (val & REG_PLL_ENABLE) {
    // //         val |= REG_PLL_LOCK;
    // //     }
    // //     break;
    // // // case REG_DMA_BGR:
    // // //     printf("YOOOP offset=%lx val=%lx size=%d\n", offset, val, size);
    // // //     val &= 0x0101;
    // // //     break;
    // // case 0x0F24 ... AW_H616_CCU_IOSIZE:
    // //     qemu_log_mask(LOG_GUEST_ERROR, "%s: out-of-bounds offset 0x%04x\n",
    // //                   __func__, (uint32_t)offset);
    // //     break;
    // default:
    //     qemu_log_mask(LOG_UNIMP, "%s: unimplemented write offset 0x%04x\n",
    //                   __func__, (uint32_t)offset);
    //     break;
    // }

    // s->regs[idx] = (uint8_t) val;
}

static const MemoryRegionOps allwinner_h616_hdmi_ops = {
    .read = allwinner_h616_hdmi_read,
    .write = allwinner_h616_hdmi_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
    .impl.min_access_size = 1,
};

static void allwinner_h616_hdmi_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    AwH616HdmiState *s = AW_H616_HDMI(obj);

    /* Memory mapping */
    memory_region_init_io(&s->iomem, OBJECT(s), &allwinner_h616_hdmi_ops, s,
                          TYPE_AW_H616_HDMI, AW_H616_HDMI_IOSIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(s), &s->irq);
}

static const VMStateDescription allwinner_h616_hdmi_vmstate = {
    .name = "allwinner-h616-dw-hdmi",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT8_ARRAY(regs, AwH616HdmiState, AW_H616_HDMI_REGS_NUM),
        VMSTATE_END_OF_LIST()
    }
};

static void allwinner_h616_hdmi_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, allwinner_h616_hdmi_reset);
    dc->vmsd = &allwinner_h616_hdmi_vmstate;
}

static const TypeInfo allwinner_h616_hdmi_info = {
    .name          = TYPE_AW_H616_HDMI,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_init = allwinner_h616_hdmi_init,
    .instance_size = sizeof(AwH616HdmiState),
    .class_init    = allwinner_h616_hdmi_class_init,
};

static void allwinner_h616_hdmi_register(void)
{
    type_register_static(&allwinner_h616_hdmi_info);
}

type_init(allwinner_h616_hdmi_register)
