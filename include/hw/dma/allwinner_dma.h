/*
 * Raspberry Pi emulation (c) 2012 Gregory Estrade
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef ALLWINNER_DMA_H
#define ALLWINNER_DMA_H

#include "hw/sysbus.h"
#include "qom/object.h"

typedef struct {
    uint32_t cs;
    uint32_t conblk_ad;
    uint32_t ti;
    uint32_t source_ad;
    uint32_t dest_ad;
    uint32_t txfr_len;
    uint32_t stride;
    uint32_t nextconbk;
    uint32_t debug;

    qemu_irq irq;
} AllwinnerDMAChan;

#define TYPE_ALLWINNER_DMA "allwinner-dma"
OBJECT_DECLARE_SIMPLE_TYPE(AllwinnerDMAState, ALLWINNER_DMA)

#define ALLWINNER_DMA_NCHANS 16

struct AllwinnerDMAState {
    /*< private >*/
    SysBusDevice busdev;
    /*< public >*/

    MemoryRegion iomem0; //, iomem15;
    MemoryRegion *dma_mr;
    AddressSpace dma_as;

    AllwinnerDMAChan chan[ALLWINNER_DMA_NCHANS];
    uint32_t int_status;
    uint32_t enable;
};

#endif
