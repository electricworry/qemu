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

#ifndef HW_DISPLAY_ALLWINNER_H616_HDMI_H
#define HW_DISPLAY_ALLWINNER_H616_HDMI_H

#include "qom/object.h"
#include "hw/sysbus.h"

/**
 * @name Constants
 * @{
 */

/** Size of register I/O address space used by CCU device */
#define AW_H616_HDMI_IOSIZE        (0x100000)

/** Total number of known registers */
#define AW_H616_HDMI_REGS_NUM      (AW_H616_HDMI_IOSIZE / sizeof(uint8_t))


#define HDMI_IH_MUTE_I2CM_STAT0                 0x0185

enum {
/* IH_I2CM_STAT0 and IH_MUTE_I2CM_STAT0 field values */
       HDMI_IH_I2CM_STAT0_DONE = 0x2,
       HDMI_IH_I2CM_STAT0_ERROR = 0x1,
/* I2CM_OPERATION field values */
       HDMI_I2CM_OPERATION_WRITE = 0x10,
       HDMI_I2CM_OPERATION_READ_EXT = 0x2,
       HDMI_I2CM_OPERATION_READ = 0x1,

};

/** @} */

/**
 * @name Object model
 * @{
 */

#define TYPE_AW_H616_HDMI    "allwinner-h616-dw-hdmi"
OBJECT_DECLARE_SIMPLE_TYPE(AwH616HdmiState, AW_H616_HDMI)

/** @} */

/**
 * Allwinner H616 CCU object instance state.
 */
struct AwH616HdmiState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    /** Interrupt output signal to notify CPU */
    qemu_irq irq;

    /** Maps I/O registers in physical memory */
    MemoryRegion iomem;

    /** Array of hardware registers */
    uint8_t regs[AW_H616_HDMI_REGS_NUM];

};

#endif /* HW_MISC_ALLWINNER_H616_HDMI_H */
