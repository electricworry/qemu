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
#include "hw/misc/allwinner-h616-ccu.h"

/* CCU register offsets */
enum {
    REG_PLL_CPUX_CTRL           = 0x0000, /* PLL CPUX Control */
    REG_PLL_DDR0_CTRL           = 0x0010, /* PLL DDR 0 Control */
    REG_PLL_DDR1_CTRL           = 0x0018, /* PLL DDR 1 Control */
    REG_PLL_PERI0_CTRL          = 0x0020, /* PLL Peripherals 0 Control */
    REG_PLL_PERI1_CTRL          = 0x0028, /* PLL Peripherals 1 Control */
    REG_PLL_GPU0_CTRL           = 0x0030, /* PLL GPU 0 Control */
    REG_PLL_VIDEO0_CTRL         = 0x0040, /* PLL Video 0 Control */
    REG_PLL_VIDEO1_CTRL         = 0x0048, /* PLL Video 1 Control */
    REG_PLL_VIDEO2_CTRL         = 0x0050, /* PLL Video 2 Control */
    REG_PLL_VE_CTRL             = 0x0058, /* PLL VE Control */
    REG_PLL_DE_CTRL             = 0x0060, /* PLL Display Engine Control */
    REG_PLL_AUDIO_CTRL          = 0x0078, /* PLL Audio Control */
    REG_PLL_DDR0_PAT_CTRL       = 0x0110, /* PLL DDR 0 Pattern Control */
    REG_PLL_DDR1_PAT_CTRL       = 0x0118, /* PLL DDR 1 Pattern Control */
    REG_PLL_PERI0_PAT0_CTRL     = 0x0120, /* PLL Peripherals 0 Pattern0 Control */
    REG_PLL_PERI0_PAT1_CTRL     = 0x0124, /* PLL Peripherals 0 Pattern1 Control */
    REG_PLL_PERI1_PAT0_CTRL     = 0x0128, /* PLL Peripherals 1 Pattern0 Control */
    REG_PLL_PERI1_PAT1_CTRL     = 0x012C, /* PLL Peripherals 1 Pattern1 Control */
    REG_PLL_GPU0_PAT0_CTRL      = 0x0130, /* PLL GPU 0 Pattern0 Control */
    REG_PLL_GPU0_PAT1_CTRL      = 0x0134, /* PLL GPU 0 Pattern1 Control */
    REG_PLL_VIDEO0_PAT0_CTRL    = 0x0140, /* PLL Video 0 Pattern0 Control */
    REG_PLL_VIDEO0_PAT1_CTRL    = 0x0144, /* PLL Video 0 Pattern1 Control */
    REG_PLL_VIDEO1_PAT0_CTRL    = 0x0148, /* PLL Video 1 Pattern0 Control */
    REG_PLL_VIDEO1_PAT1_CTRL    = 0x014C, /* PLL Video 1 Pattern1 Control */
    REG_PLL_VIDEO2_PAT0_CTRL    = 0x0150, /* PLL Video 2 Pattern0 Control */
    REG_PLL_VIDEO2_PAT1_CTRL    = 0x0154, /* PLL Video 2 Pattern1 Control */
    REG_PLL_VE_PAT0_CTRL        = 0x0158, /* PLL VE Pattern0 Control */
    REG_PLL_VE_PAT1_CTRL        = 0x015C, /* PLL VE Pattern1 Control */
    REG_PLL_DE_PAT0_CTRL        = 0x0160, /* PLL Display Engine Pattern0 Control */
    REG_PLL_DE_PAT1_CTRL        = 0x0164, /* PLL Display Engine Pattern1 Control */
    REG_PLL_AUDIO_PAT0_CTRL     = 0x0178, /* PLL Audio Pattern0 Control */
    REG_PLL_AUDIO_PAT1_CTRL     = 0x017C, /* PLL Audio Pattern1 Control */
    REG_PLL_CPUX_BIAS           = 0x0300, /* PLL CPUX Bias */
    REG_PLL_DDR0_BIAS           = 0x0310, /* PLL DDR 0 Bias */
    REG_PLL_DDR1_BIAS           = 0x0318, /* PLL DDR 1 Bias */
    REG_PLL_PERI0_BIAS          = 0x0320, /* PLL Peripherals 0 Bias */
    REG_PLL_PERI1_BIAS          = 0x0328, /* PLL Peripherals 1 Bias */
    REG_PLL_GPU0_BIAS           = 0x0330, /* PLL GPU 0 Bias */
    REG_PLL_VIDEO0_BIAS         = 0x0340, /* PLL Video 0 Bias */
    REG_PLL_VIDEO1_BIAS         = 0x0348, /* PLL Video 1 Bias */
    REG_PLL_VIDEO2_BIAS         = 0x0350, /* PLL Video 2 Bias */
    REG_PLL_VE_BIAS             = 0x0358, /* PLL VE Bias */
    REG_PLL_DE_BIAS             = 0x0360, /* PLL Display Engine Bias */
    REG_PLL_AUDIO_BIAS          = 0x0378, /* PLL Audio Bias */
    REG_PLL_CPUX_TUNING         = 0x0400, /* PLL CPUX Tuning */
    REG_CPUX_AXI_CFG            = 0x0500, /* CPUX AXI Configuration */
    REG_PSI_AHB1_AHB2_CFG       = 0x0510, /* PSI_AHB1_AHB2 Configuration Register */
    REG_AHB3_CFG                = 0x051C, /* AHB3 Configuration Register */
    REG_APB1_CFG                = 0x0520, /* APB1 Configuration */
    REG_APB2_CFG                = 0x0524, /* APB2 Configuration */
    REG_MBUS_CFG                = 0x0540, /* MBUS Configuration */
    REG_DE_CLK                  = 0x0600, /* DE Clock */
    REG_DE_BGR                  = 0x060C, /* DE Bus Gating Reset */
    REG_DI_CLK                  = 0x0620, /* DI Clock */
    REG_DI_BGR                  = 0x062C, /* DI Bus Gating Reset */
    REG_G2D_CLK                 = 0x0630, /* G2D Clock */
    REG_G2D_BGR                 = 0x063C, /* G2D Bus Gating Reset */
    REG_GPU_CLK0                = 0x0670, /* GPU Clock 0 */
    REG_GPU_CLK1                = 0x0674, /* GPU Clock1 */
    REG_GPU_BGR                 = 0x067C, /* GPU Bus Gating Reset */
    REG_CE_CLK                  = 0x0680, /* CE Clock */
    REG_CE_BGR                  = 0x068C, /* CE Bus Gating Reset */
    REG_VE_CLK                  = 0x0690, /* VE Clock */
    REG_VE_BGR                  = 0x069C, /* VE Bus Gating Reset */
    REG_DMA_BGR                 = 0x070C, /* DMA Bus Gating Reset */
    REG_HSTIMER_BGR             = 0x073C, /* HSTIMER Bus Gating Reset */
    REG_AVS_CLK                 = 0x0740, /* AVS Clock */
    REG_DBGSYS_BGR              = 0x078C, /* DBGSYS Bus Gating Reset */
    REG_PSI_BGR                 = 0x079C, /* PSI Bus Gating Reset */
    REG_PWM_BGR                 = 0x07AC, /* PWM Bus Gating Reset */
    REG_IOMMU_BGR               = 0x07BC, /* IOMMU Bus Gating Reset */
    REG_DRAM_CLK                = 0x0800, /* DRAM Clock */
    REG_MBUS_MAT_CLK_GATING     = 0x0804, /* MBUS Master Clock Gating */
    REG_DRAM_BGR                = 0x080C, /* DRAM Bus Gating Reset */
    REG_NAND0_0_CLK             = 0x0810, /* NAND0_0 Clock */
    REG_NAND0_1_CLK             = 0x0814, /* NAND0_1 Clock */
    REG_NAND_BGR                = 0x082C, /* NAND Bus Gating Reset */
    REG_SMHC0_CLK               = 0x0830, /* SMHC0 Clock */
    REG_SMHC1_CLK               = 0x0834, /* SMHC1 Clock */
    REG_SMHC2_CLK               = 0x0838, /* SMHC2 Clock */
    REG_SMHC_BGR                = 0x084C, /* SMHC Bus Gating Reset */
    REG_UART_BGR                = 0x090C, /* UART Bus Gating Reset */
    REG_TWI_BGR                 = 0x091C, /* TWI Bus Gating Reset */
    REG_SPI0_CLK                = 0x0940, /* SPI0 Clock */
    REG_SPI1_CLK                = 0x0944, /* SPI1 Clock */
    REG_SPI_BGR                 = 0x096C, /* SPI Bus Gating Reset */
    REG_EPHY_25M_CLK            = 0x0970, /* EPHY_25M Clock */
    REG_EMAC_BGR                = 0x097C, /* EMAC Bus Gating Reset */
    REG_TS_CLK                  = 0x09B0, /* TS Clock */
    REG_TS_BGR                  = 0x09BC, /* TS Bus Gating Reset */
    REG_THS_BGR                 = 0x09FC, /* THS Bus Gating Reset */
    REG_OWA_CLK                 = 0x0A20, /* OWA Clock */
    REG_OWA_BGR                 = 0x0A2C, /* OWA Bus Gating Reset */
    REG_DMIC_CLK                = 0x0A40, /* DMIC Clock */
    REG_DMIC_BGR                = 0x0A4C, /* DMIC Bus Gating Reset */
    REG_AUDIO_CODEC_1X_CLK      = 0x0A50, /* AUDIO CODEC 1X Clock */
    REG_AUDIO_CODEC_4X_CLK      = 0x0A54, /* AUDIO CODEC 4X Clock */
    REG_AUDIO_CODEC_BGR         = 0x0A5C, /* AUDIO CODEC Bus Gating Reset */
    REG_AUDIO_HUB_CLK           = 0x0A60, /* AUDIO_HUB Clock */
    REG_AUDIO_HUB_BGR           = 0x0A6C, /* AUDIO_HUB Bus Gating Reset */
    REG_USB0_CLK                = 0x0A70, /* USB0 Clock */
    REG_USB1_CLK                = 0x0A74, /* USB1 Clock */
    REG_USB2_CLK                = 0x0A78, /* USB2 Clock */
    REG_USB3_CLK                = 0x0A7C, /* USB3 Clock */
    REG_USB_BGR                 = 0x0A8C, /* USB Bus Gating Reset */
    REG_LRADC_BGR               = 0x0A9C, /* LRADC Bus Gating Reset */
    REG_HDMI0_CLK               = 0x0B00, /* HDMI0 Clock */
    REG_HDMI0_SLOW_CLK          = 0x0B04, /* HDMI0 Slow Clock */
    REG_HDMI_CEC_CLK            = 0x0B10, /* HDMI CEC Clock */
    REG_HDMI_BGR                = 0x0B1C, /* HDMI Bus Gating Reset */
    REG_DISPLAY_IF_TOP_BGR      = 0x0B5C, /* DISPLAY_IF_TOP BUS GATING RESET */
    REG_TCON_TV0_CLK            = 0x0B80, /* TCON TV0 Clock */
    REG_TCON_TV1_CLK            = 0x0B84, /* TCON TV1 Clock */
    REG_TCON_TV_BGR             = 0x0B9C, /* TCON TV GATING RESET */
    REG_TVE0_CLK                = 0x0BB0, /* TVE0 Clock */
    REG_TVE_BGR                 = 0x0BBC, /* TVE BUS GATING RESET */
    REG_HDMI_HDCP_CLK           = 0x0C40, /* HDMI HDCP Clock */
    REG_HDMI_HDCP_BGR           = 0x0C4C, /* HDMI HDCP Bus Gating Reset */
    REG_CCU_SEC_SWITCH          = 0x0F00, /* CCU Security Switch */
    REG_PLL_LOCK_DBG_CTRL       = 0x0F04, /* PLL Lock Debug Control */
    REG_FRE_DET_CTRL            = 0x0F08, /* Frequency Detect Control */
    REG_FRE_UP_LIM              = 0x0F0C, /* Frequency Up Limit */
    REG_FRE_DOWN_LIM            = 0x0F10, /* Frequency Down Limit */
    REG_24M_27M_CLK_OUTPUT      = 0x0F20, /* 24M or 27M Clock Output */
};

#define REG_INDEX(offset)    (offset / sizeof(uint32_t))

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
    REG_PLL_CPUX_CTRL_RST           = 0x0A001000, /* PLL CPUX Control */
    REG_PLL_DDR0_CTRL_RST           = 0x08002301, /* PLL DDR 0 Control */
    REG_PLL_DDR1_CTRL_RST           = 0x08002301, /* PLL DDR 1 Control */
    REG_PLL_PERI0_CTRL_RST          = 0x08003100, /* PLL Peripherals 0 Control */
    REG_PLL_PERI1_CTRL_RST          = 0x08003100, /* PLL Peripherals 1 Control */
    REG_PLL_GPU0_CTRL_RST           = 0x08003100, /* PLL GPU 0 Control */
    REG_PLL_VIDEO0_CTRL_RST         = 0x08006203, /* PLL Video 0 Control */
    REG_PLL_VIDEO1_CTRL_RST         = 0x08006203, /* PLL Video 1 Control */
    REG_PLL_VIDEO2_CTRL_RST         = 0x08006203, /* PLL Video 2 Control */
    REG_PLL_VE_CTRL_RST             = 0x08002301, /* PLL VE Control */
    REG_PLL_DE_CTRL_RST             = 0x08002301, /* PLL Display Engine Control */
    REG_PLL_AUDIO_CTRL_RST          = 0x08142A01, /* PLL Audio Control */
    REG_PLL_DDR0_PAT_CTRL_RST       = 0x08142A01, /* PLL DDR 0 Pattern Control */
    REG_PLL_DDR1_PAT_CTRL_RST       = 0x00000000, /* PLL DDR 1 Pattern Control */
    REG_PLL_PERI0_PAT0_CTRL_RST     = 0x00000000, /* PLL Peripherals 0 Pattern0 Control */
    REG_PLL_PERI0_PAT1_CTRL_RST     = 0x00000000, /* PLL Peripherals 0 Pattern1 Control */
    REG_PLL_PERI1_PAT0_CTRL_RST     = 0x00000000, /* PLL Peripherals 1 Pattern0 Control */
    REG_PLL_PERI1_PAT1_CTRL_RST     = 0x00000000, /* PLL Peripherals 1 Pattern1 Control */
    REG_PLL_GPU0_PAT0_CTRL_RST      = 0x00000000, /* PLL GPU 0 Pattern0 Control */
    REG_PLL_GPU0_PAT1_CTRL_RST      = 0x00000000, /* PLL GPU 0 Pattern1 Control */
    REG_PLL_VIDEO0_PAT0_CTRL_RST    = 0x00000000, /* PLL Video 0 Pattern0 Control */
    REG_PLL_VIDEO0_PAT1_CTRL_RST    = 0x00000000, /* PLL Video 0 Pattern1 Control */
    REG_PLL_VIDEO1_PAT0_CTRL_RST    = 0x00000000, /* PLL Video 1 Pattern0 Control */
    REG_PLL_VIDEO1_PAT1_CTRL_RST    = 0x00000000, /* PLL Video 1 Pattern1 Control */
    REG_PLL_VIDEO2_PAT0_CTRL_RST    = 0x00000000, /* PLL Video 2 Pattern0 Control */
    REG_PLL_VIDEO2_PAT1_CTRL_RST    = 0x00000000, /* PLL Video 2 Pattern1 Control */
    REG_PLL_VE_PAT0_CTRL_RST        = 0x00000000, /* PLL VE Pattern0 Control */
    REG_PLL_VE_PAT1_CTRL_RST        = 0x00000000, /* PLL VE Pattern1 Control */
    REG_PLL_DE_PAT0_CTRL_RST        = 0x00000000, /* PLL Display Engine Pattern0 Control */
    REG_PLL_DE_PAT1_CTRL_RST        = 0x00000000, /* PLL Display Engine Pattern1 Control */
    REG_PLL_AUDIO_PAT0_CTRL_RST     = 0x00000000, /* PLL Audio Pattern0 Control */
    REG_PLL_AUDIO_PAT1_CTRL_RST     = 0x00000000, /* PLL Audio Pattern1 Control */
    REG_PLL_CPUX_BIAS_RST           = 0x80100000, /* PLL CPUX Bias */
    REG_PLL_DDR0_BIAS_RST           = 0x00030000, /* PLL DDR 0 Bias */
    REG_PLL_DDR1_BIAS_RST           = 0x00030000, /* PLL DDR 1 Bias TODO: NOT IN SPEC */
    REG_PLL_PERI0_BIAS_RST          = 0x00030000, /* PLL Peripherals 0 Bias */
    REG_PLL_PERI1_BIAS_RST          = 0x00030000, /* PLL Peripherals 1 Bias */
    REG_PLL_GPU0_BIAS_RST           = 0x00030000, /* PLL GPU 0 Bias */
    REG_PLL_VIDEO0_BIAS_RST         = 0x00030000, /* PLL Video 0 Bias */
    REG_PLL_VIDEO1_BIAS_RST         = 0x00030000, /* PLL Video 1 Bias */
    REG_PLL_VIDEO2_BIAS_RST         = 0x00030000, /* PLL Video 2 Bias */
    REG_PLL_VE_BIAS_RST             = 0x00030000, /* PLL VE Bias */
    REG_PLL_DE_BIAS_RST             = 0x00030000, /* PLL Display Engine Bias */
    REG_PLL_AUDIO_BIAS_RST          = 0x00030000, /* PLL Audio Bias */
    REG_PLL_CPUX_TUNING_RST         = 0x44404000, /* PLL CPUX Tuning */
    REG_CPUX_AXI_CFG_RST            = 0x00000301, /* CPUX AXI Configuration */
    REG_PSI_AHB1_AHB2_CFG_RST       = 0x00000000, /* PSI_AHB1_AHB2 Configuration Register */
    REG_AHB3_CFG_RST                = 0x00000000, /* AHB3 Configuration Register */
    REG_APB1_CFG_RST                = 0x00000000, /* APB1 Configuration */
    REG_APB2_CFG_RST                = 0x00000000, /* APB2 Configuration */
    REG_MBUS_CFG_RST                = 0xC0000000, /* MBUS Configuration */
    REG_DE_CLK_RST                  = 0x00000000, /* DE Clock */
    REG_DE_BGR_RST                  = 0x00000000, /* DE Bus Gating Reset */
    REG_DI_CLK_RST                  = 0x00000000, /* DI Clock */
    REG_DI_BGR_RST                  = 0x00000000, /* DI Bus Gating Reset */
    REG_G2D_CLK_RST                 = 0x00000000, /* G2D Clock */
    REG_G2D_BGR_RST                 = 0x00000000, /* G2D Bus Gating Reset */
    REG_GPU_CLK0_RST                = 0x00000000, /* GPU Clock 0 */
    REG_GPU_CLK1_RST                = 0x00000000, /* GPU Clock1 */
    REG_GPU_BGR_RST                 = 0x00000000, /* GPU Bus Gating Reset */
    REG_CE_CLK_RST                  = 0x00000000, /* CE Clock */
    REG_CE_BGR_RST                  = 0x00000000, /* CE Bus Gating Reset */
    REG_VE_CLK_RST                  = 0x00000000, /* VE Clock */
    REG_VE_BGR_RST                  = 0x00000000, /* VE Bus Gating Reset */
    REG_DMA_BGR_RST                 = 0x00000000, /* DMA Bus Gating Reset */
    REG_HSTIMER_BGR_RST             = 0x00000000, /* HSTIMER Bus Gating Reset */
    REG_AVS_CLK_RST                 = 0x00000000, /* AVS Clock */
    REG_DBGSYS_BGR_RST              = 0x00000000, /* DBGSYS Bus Gating Reset */
    REG_PSI_BGR_RST                 = 0x00000000, /* PSI Bus Gating Reset */
    REG_PWM_BGR_RST                 = 0x00000000, /* PWM Bus Gating Reset */
    REG_IOMMU_BGR_RST               = 0x00000000, /* IOMMU Bus Gating Reset */
    REG_DRAM_CLK_RST                = 0x00000000, /* DRAM Clock */
    REG_MBUS_MAT_CLK_GATING_RST     = 0x00000000, /* MBUS Master Clock Gating */
    REG_DRAM_BGR_RST                = 0x00000000, /* DRAM Bus Gating Reset */
    REG_NAND0_0_CLK_RST             = 0x00000000, /* NAND0_0 Clock */
    REG_NAND0_1_CLK_RST             = 0x00000000, /* NAND0_1 Clock */
    REG_NAND_BGR_RST                = 0x00000000, /* NAND Bus Gating Reset */
    REG_SMHC0_CLK_RST               = 0x00000000, /* SMHC0 Clock */
    REG_SMHC1_CLK_RST               = 0x00000000, /* SMHC1 Clock */
    REG_SMHC2_CLK_RST               = 0x00000000, /* SMHC2 Clock */
    REG_SMHC_BGR_RST                = 0x00000000, /* SMHC Bus Gating Reset */
    REG_UART_BGR_RST                = 0x00000000, /* UART Bus Gating Reset */
    REG_TWI_BGR_RST                 = 0x00000000, /* TWI Bus Gating Reset */
    REG_SPI0_CLK_RST                = 0x00000000, /* SPI0 Clock */
    REG_SPI1_CLK_RST                = 0x00000000, /* SPI1 Clock */
    REG_SPI_BGR_RST                 = 0x00000000, /* SPI Bus Gating Reset */
    REG_EPHY_25M_CLK_RST            = 0x00000000, /* EPHY_25M Clock */
    REG_EMAC_BGR_RST                = 0x00000000, /* EMAC Bus Gating Reset */
    REG_TS_CLK_RST                  = 0x00000000, /* TS Clock */
    REG_TS_BGR_RST                  = 0x00000000, /* TS Bus Gating Reset */
    REG_THS_BGR_RST                 = 0x00000000, /* THS Bus Gating Reset */
    REG_OWA_CLK_RST                 = 0x00000000, /* OWA Clock */
    REG_OWA_BGR_RST                 = 0x00000000, /* OWA Bus Gating Reset */
    REG_DMIC_CLK_RST                = 0x00000000, /* DMIC Clock */
    REG_DMIC_BGR_RST                = 0x00000000, /* DMIC Bus Gating Reset */
    REG_AUDIO_CODEC_1X_CLK_RST      = 0x00000000, /* AUDIO CODEC 1X Clock */
    REG_AUDIO_CODEC_4X_CLK_RST      = 0x00000000, /* AUDIO CODEC 4X Clock */
    REG_AUDIO_CODEC_BGR_RST         = 0x00000000, /* AUDIO CODEC Bus Gating Reset */
    REG_AUDIO_HUB_CLK_RST           = 0x00000000, /* AUDIO_HUB Clock */
    REG_AUDIO_HUB_BGR_RST           = 0x00000000, /* AUDIO_HUB Bus Gating Reset */
    REG_USB0_CLK_RST                = 0x00000000, /* USB0 Clock */
    REG_USB1_CLK_RST                = 0x00000000, /* USB1 Clock */
    REG_USB2_CLK_RST                = 0x00000000, /* USB2 Clock */
    REG_USB3_CLK_RST                = 0x00000000, /* USB3 Clock */
    REG_USB_BGR_RST                 = 0x00000000, /* USB Bus Gating Reset */
    REG_LRADC_BGR_RST               = 0x00000000, /* LRADC Gating Reset */
    REG_HDMI0_CLK_RST               = 0x00000000, /* HDMI0 Clock */
    REG_HDMI0_SLOW_CLK_RST          = 0x00000000, /* HDMI0 Slow Clock */
    REG_HDMI_CEC_CLK_RST            = 0x00000000, /* HDMI CEC Clock */
    REG_HDMI_BGR_RST                = 0x00000000, /* HDMI Bus Gating Reset */
    REG_DISPLAY_IF_TOP_BGR_RST      = 0x00000000, /* DISPLAY_IF_TOP BUS GATING RESET */
    REG_TCON_TV0_CLK_RST            = 0x00000000, /* TCON TV0 Clock */
    REG_TCON_TV1_CLK_RST            = 0x00000000, /* TCON TV1 Clock */
    REG_TCON_TV_BGR_RST             = 0x00000000, /* TCON TV GATING RESET */
    REG_TVE0_CLK_RST                = 0x00000000, /* TVE0 Clock */
    REG_TVE_BGR_RST                 = 0x00000000, /* TVE BUS GATING RESET */
    REG_HDMI_HDCP_CLK_RST           = 0x00000000, /* HDMI HDCP Clock */
    REG_HDMI_HDCP_BGR_RST           = 0x00000000, /* HDMI HDCP Bus Gating Reset */
    REG_CCU_SEC_SWITCH_RST          = 0x00000000, /* CCU Security Switch */
    REG_PLL_LOCK_DBG_CTRL_RST       = 0x00000000, /* PLL Lock Debug Control */
    REG_FRE_DET_CTRL_RST            = 0x00000020, /* Frequency Detect Control */
    REG_FRE_UP_LIM_RST              = 0x00000000, /* Frequency Up Limit */
    REG_FRE_DOWN_LIM_RST            = 0x00000000, /* Frequency Down Limit */
    REG_24M_27M_CLK_OUTPUT_RST      = 0x00000000, /* 24M or 27M Clock Output */
};

static uint64_t allwinner_h616_ccu_read(void *opaque, hwaddr offset,
                                      unsigned size)
{
    const AwH616ClockCtlState *s = AW_H616_CCU(opaque);
    const uint32_t idx = REG_INDEX(offset);

    switch (offset) {
    case 0x0F24 ... AW_H616_CCU_IOSIZE:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: out-of-bounds offset 0x%04x\n",
                      __func__, (uint32_t)offset);
        return 0;
    }

    printf("CCUREAD: addr 0x%x size %d val 0x%x\n", (unsigned int) offset, size, s->regs[idx]);
    return s->regs[idx];
}

static void allwinner_h616_ccu_write(void *opaque, hwaddr offset,
                                   uint64_t val, unsigned size)
{
    printf("CCUWRITE: addr 0x%x size %d val 0x%lx\n", (unsigned int) offset, size, val);
    AwH616ClockCtlState *s = AW_H616_CCU(opaque);
    const uint32_t idx = REG_INDEX(offset);

    switch (offset) {
    // case REG_DRAM_CFG:    /* DRAM Configuration */
    //     val &= ~REG_DRAM_CFG_UPDATE;
    //     break;
    case REG_PLL_CPUX_CTRL:    /* PLL CPUX Control */
    case REG_PLL_DDR0_CTRL:    /* PLL DDR 0 Control */
    case REG_PLL_DDR1_CTRL:    /* PLL DDR 1 Control */
    case REG_PLL_PERI0_CTRL:   /* PLL Peripherals 0 Control */
    case REG_PLL_PERI1_CTRL:   /* PLL Peripherals 1 Control */
    case REG_PLL_GPU0_CTRL:    /* PLL GPU 0 Control */
    case REG_PLL_VIDEO0_CTRL:  /* PLL Video 0 Control */
    case REG_PLL_VIDEO1_CTRL:  /* PLL Video 1 Control */
    case REG_PLL_VIDEO2_CTRL:  /* PLL Video 2 Control */
    case REG_PLL_VE_CTRL:      /* PLL VE Control */
    case REG_PLL_DE_CTRL:      /* PLL Display Engine Control */
    case REG_PLL_AUDIO_CTRL:   /* PLL Audio Control */
        if (val & REG_PLL_ENABLE) {
            val |= REG_PLL_LOCK;
        }
        break;
    // case REG_DMA_BGR:
    //     printf("YOOOP offset=%lx val=%lx size=%d\n", offset, val, size);
    //     val &= 0x0101;
    //     break;
    case 0x0F24 ... AW_H616_CCU_IOSIZE:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: out-of-bounds offset 0x%04x\n",
                      __func__, (uint32_t)offset);
        break;
    default:
        qemu_log_mask(LOG_UNIMP, "%s: unimplemented write offset 0x%04x\n",
                      __func__, (uint32_t)offset);
        break;
    }

    s->regs[idx] = (uint32_t) val;
}

static const MemoryRegionOps allwinner_h616_ccu_ops = {
    .read = allwinner_h616_ccu_read,
    .write = allwinner_h616_ccu_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .impl.min_access_size = 4,
};

static void allwinner_h616_ccu_reset(DeviceState *dev)
{
    AwH616ClockCtlState *s = AW_H616_CCU(dev);

    /* Set default values for registers */
    s->regs[REG_INDEX(REG_PLL_CPUX_CTRL)]           = REG_PLL_CPUX_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_DDR0_CTRL)]           = REG_PLL_DDR0_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_DDR1_CTRL)]           = REG_PLL_DDR1_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_PERI0_CTRL)]          = REG_PLL_PERI0_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_PERI1_CTRL)]          = REG_PLL_PERI1_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_GPU0_CTRL)]           = REG_PLL_GPU0_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_VIDEO0_CTRL)]         = REG_PLL_VIDEO0_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_VIDEO1_CTRL)]         = REG_PLL_VIDEO1_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_VIDEO2_CTRL)]         = REG_PLL_VIDEO2_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_VE_CTRL)]             = REG_PLL_VE_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_DE_CTRL)]             = REG_PLL_DE_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_AUDIO_CTRL)]          = REG_PLL_AUDIO_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_DDR0_PAT_CTRL)]       = REG_PLL_DDR0_PAT_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_DDR1_PAT_CTRL)]       = REG_PLL_DDR1_PAT_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_PERI0_PAT0_CTRL)]     = REG_PLL_PERI0_PAT0_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_PERI0_PAT1_CTRL)]     = REG_PLL_PERI0_PAT1_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_PERI1_PAT0_CTRL)]     = REG_PLL_PERI1_PAT0_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_PERI1_PAT1_CTRL)]     = REG_PLL_PERI1_PAT1_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_GPU0_PAT0_CTRL)]      = REG_PLL_GPU0_PAT0_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_GPU0_PAT1_CTRL)]      = REG_PLL_GPU0_PAT1_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_VIDEO0_PAT0_CTRL)]    = REG_PLL_VIDEO0_PAT0_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_VIDEO0_PAT1_CTRL)]    = REG_PLL_VIDEO0_PAT1_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_VIDEO1_PAT0_CTRL)]    = REG_PLL_VIDEO1_PAT0_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_VIDEO1_PAT1_CTRL)]    = REG_PLL_VIDEO1_PAT1_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_VIDEO2_PAT0_CTRL)]    = REG_PLL_VIDEO2_PAT0_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_VIDEO2_PAT1_CTRL)]    = REG_PLL_VIDEO2_PAT1_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_VE_PAT0_CTRL)]        = REG_PLL_VE_PAT0_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_VE_PAT1_CTRL)]        = REG_PLL_VE_PAT1_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_DE_PAT0_CTRL)]        = REG_PLL_DE_PAT0_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_DE_PAT1_CTRL)]        = REG_PLL_DE_PAT1_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_AUDIO_PAT0_CTRL)]     = REG_PLL_AUDIO_PAT0_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_AUDIO_PAT1_CTRL)]     = REG_PLL_AUDIO_PAT1_CTRL_RST;
    s->regs[REG_INDEX(REG_PLL_CPUX_BIAS)]           = REG_PLL_CPUX_BIAS_RST;
    s->regs[REG_INDEX(REG_PLL_DDR0_BIAS)]           = REG_PLL_DDR0_BIAS_RST;
    s->regs[REG_INDEX(REG_PLL_DDR1_BIAS)]           = REG_PLL_DDR1_BIAS_RST;
    s->regs[REG_INDEX(REG_PLL_PERI0_BIAS)]          = REG_PLL_PERI0_BIAS_RST;
    s->regs[REG_INDEX(REG_PLL_PERI1_BIAS)]          = REG_PLL_PERI1_BIAS_RST;
    s->regs[REG_INDEX(REG_PLL_GPU0_BIAS)]           = REG_PLL_GPU0_BIAS_RST;
    s->regs[REG_INDEX(REG_PLL_VIDEO0_BIAS)]         = REG_PLL_VIDEO0_BIAS_RST;
    s->regs[REG_INDEX(REG_PLL_VIDEO1_BIAS)]         = REG_PLL_VIDEO1_BIAS_RST;
    s->regs[REG_INDEX(REG_PLL_VIDEO2_BIAS)]         = REG_PLL_VIDEO2_BIAS_RST;
    s->regs[REG_INDEX(REG_PLL_VE_BIAS)]             = REG_PLL_VE_BIAS_RST;
    s->regs[REG_INDEX(REG_PLL_DE_BIAS)]             = REG_PLL_DE_BIAS_RST;
    s->regs[REG_INDEX(REG_PLL_AUDIO_BIAS)]          = REG_PLL_AUDIO_BIAS_RST;
    s->regs[REG_INDEX(REG_PLL_CPUX_TUNING)]         = REG_PLL_CPUX_TUNING_RST;
    s->regs[REG_INDEX(REG_CPUX_AXI_CFG)]            = REG_CPUX_AXI_CFG_RST;
    s->regs[REG_INDEX(REG_PSI_AHB1_AHB2_CFG)]       = REG_PSI_AHB1_AHB2_CFG_RST;
    s->regs[REG_INDEX(REG_AHB3_CFG)]                = REG_AHB3_CFG_RST;
    s->regs[REG_INDEX(REG_APB1_CFG)]                = REG_APB1_CFG_RST;
    s->regs[REG_INDEX(REG_APB2_CFG)]                = REG_APB2_CFG_RST;
    s->regs[REG_INDEX(REG_MBUS_CFG)]                = REG_MBUS_CFG_RST;
    s->regs[REG_INDEX(REG_DE_CLK)]                  = REG_DE_CLK_RST;
    s->regs[REG_INDEX(REG_DE_BGR)]                  = REG_DE_BGR_RST;
    s->regs[REG_INDEX(REG_DI_CLK)]                  = REG_DI_CLK_RST;
    s->regs[REG_INDEX(REG_DI_BGR)]                  = REG_DI_BGR_RST;
    s->regs[REG_INDEX(REG_G2D_CLK)]                 = REG_G2D_CLK_RST;
    s->regs[REG_INDEX(REG_G2D_BGR)]                 = REG_G2D_BGR_RST;
    s->regs[REG_INDEX(REG_GPU_CLK0)]                = REG_GPU_CLK0_RST;
    s->regs[REG_INDEX(REG_GPU_CLK1)]                = REG_GPU_CLK1_RST;
    s->regs[REG_INDEX(REG_GPU_BGR)]                 = REG_GPU_BGR_RST;
    s->regs[REG_INDEX(REG_CE_CLK)]                  = REG_CE_CLK_RST;
    s->regs[REG_INDEX(REG_CE_BGR)]                  = REG_CE_BGR_RST;
    s->regs[REG_INDEX(REG_VE_CLK)]                  = REG_VE_CLK_RST;
    s->regs[REG_INDEX(REG_VE_BGR)]                  = REG_VE_BGR_RST;
    s->regs[REG_INDEX(REG_DMA_BGR)]                 = REG_DMA_BGR_RST;
    s->regs[REG_INDEX(REG_HSTIMER_BGR)]             = REG_HSTIMER_BGR_RST;
    s->regs[REG_INDEX(REG_AVS_CLK)]                 = REG_AVS_CLK_RST;
    s->regs[REG_INDEX(REG_DBGSYS_BGR)]              = REG_DBGSYS_BGR_RST;
    s->regs[REG_INDEX(REG_PSI_BGR)]                 = REG_PSI_BGR_RST;
    s->regs[REG_INDEX(REG_PWM_BGR)]                 = REG_PWM_BGR_RST;
    s->regs[REG_INDEX(REG_IOMMU_BGR)]               = REG_IOMMU_BGR_RST;
    s->regs[REG_INDEX(REG_DRAM_CLK)]                = REG_DRAM_CLK_RST;
    s->regs[REG_INDEX(REG_MBUS_MAT_CLK_GATING)]     = REG_MBUS_MAT_CLK_GATING_RST;
    s->regs[REG_INDEX(REG_DRAM_BGR)]                = REG_DRAM_BGR_RST;
    s->regs[REG_INDEX(REG_NAND0_0_CLK)]             = REG_NAND0_0_CLK_RST;
    s->regs[REG_INDEX(REG_NAND0_1_CLK)]             = REG_NAND0_1_CLK_RST;
    s->regs[REG_INDEX(REG_NAND_BGR)]                = REG_NAND_BGR_RST;
    s->regs[REG_INDEX(REG_SMHC0_CLK)]               = REG_SMHC0_CLK_RST;
    s->regs[REG_INDEX(REG_SMHC1_CLK)]               = REG_SMHC1_CLK_RST;
    s->regs[REG_INDEX(REG_SMHC2_CLK)]               = REG_SMHC2_CLK_RST;
    s->regs[REG_INDEX(REG_SMHC_BGR)]                = REG_SMHC_BGR_RST;
    s->regs[REG_INDEX(REG_UART_BGR)]                = REG_UART_BGR_RST;
    s->regs[REG_INDEX(REG_TWI_BGR)]                 = REG_TWI_BGR_RST;
    s->regs[REG_INDEX(REG_SPI0_CLK)]                = REG_SPI0_CLK_RST;
    s->regs[REG_INDEX(REG_SPI1_CLK)]                = REG_SPI1_CLK_RST;
    s->regs[REG_INDEX(REG_SPI_BGR)]                 = REG_SPI_BGR_RST;
    s->regs[REG_INDEX(REG_EPHY_25M_CLK)]            = REG_EPHY_25M_CLK_RST;
    s->regs[REG_INDEX(REG_EMAC_BGR)]                = REG_EMAC_BGR_RST;
    s->regs[REG_INDEX(REG_TS_CLK)]                  = REG_TS_CLK_RST;
    s->regs[REG_INDEX(REG_TS_BGR)]                  = REG_TS_BGR_RST;
    s->regs[REG_INDEX(REG_THS_BGR)]                 = REG_THS_BGR_RST;
    s->regs[REG_INDEX(REG_OWA_CLK)]                 = REG_OWA_CLK_RST;
    s->regs[REG_INDEX(REG_OWA_BGR)]                 = REG_OWA_BGR_RST;
    s->regs[REG_INDEX(REG_DMIC_CLK)]                = REG_DMIC_CLK_RST;
    s->regs[REG_INDEX(REG_DMIC_BGR)]                = REG_DMIC_BGR_RST;
    s->regs[REG_INDEX(REG_AUDIO_CODEC_1X_CLK)]      = REG_AUDIO_CODEC_1X_CLK_RST;
    s->regs[REG_INDEX(REG_AUDIO_CODEC_4X_CLK)]      = REG_AUDIO_CODEC_4X_CLK_RST;
    s->regs[REG_INDEX(REG_AUDIO_CODEC_BGR)]         = REG_AUDIO_CODEC_BGR_RST;
    s->regs[REG_INDEX(REG_AUDIO_HUB_CLK)]           = REG_AUDIO_HUB_CLK_RST;
    s->regs[REG_INDEX(REG_AUDIO_HUB_BGR)]           = REG_AUDIO_HUB_BGR_RST;
    s->regs[REG_INDEX(REG_USB0_CLK)]                = REG_USB0_CLK_RST;
    s->regs[REG_INDEX(REG_USB1_CLK)]                = REG_USB1_CLK_RST;
    s->regs[REG_INDEX(REG_USB2_CLK)]                = REG_USB2_CLK_RST;
    s->regs[REG_INDEX(REG_USB3_CLK)]                = REG_USB3_CLK_RST;
    s->regs[REG_INDEX(REG_USB_BGR)]                 = REG_USB_BGR_RST;
    s->regs[REG_INDEX(REG_LRADC_BGR)]               = REG_LRADC_BGR_RST;
    s->regs[REG_INDEX(REG_HDMI0_CLK)]               = REG_HDMI0_CLK_RST;
    s->regs[REG_INDEX(REG_HDMI0_SLOW_CLK)]          = REG_HDMI0_SLOW_CLK_RST;
    s->regs[REG_INDEX(REG_HDMI_CEC_CLK)]            = REG_HDMI_CEC_CLK_RST;
    s->regs[REG_INDEX(REG_HDMI_BGR)]                = REG_HDMI_BGR_RST;
    s->regs[REG_INDEX(REG_DISPLAY_IF_TOP_BGR)]      = REG_DISPLAY_IF_TOP_BGR_RST;
    s->regs[REG_INDEX(REG_TCON_TV0_CLK)]            = REG_TCON_TV0_CLK_RST;
    s->regs[REG_INDEX(REG_TCON_TV1_CLK)]            = REG_TCON_TV1_CLK_RST;
    s->regs[REG_INDEX(REG_TCON_TV_BGR)]             = REG_TCON_TV_BGR_RST;
    s->regs[REG_INDEX(REG_TVE0_CLK)]                = REG_TVE0_CLK_RST;
    s->regs[REG_INDEX(REG_TVE_BGR)]                 = REG_TVE_BGR_RST;
    s->regs[REG_INDEX(REG_HDMI_HDCP_CLK)]           = REG_HDMI_HDCP_CLK_RST;
    s->regs[REG_INDEX(REG_HDMI_HDCP_BGR)]           = REG_HDMI_HDCP_BGR_RST;
    s->regs[REG_INDEX(REG_CCU_SEC_SWITCH)]          = REG_CCU_SEC_SWITCH_RST;
    s->regs[REG_INDEX(REG_PLL_LOCK_DBG_CTRL)]       = REG_PLL_LOCK_DBG_CTRL_RST;
    s->regs[REG_INDEX(REG_FRE_DET_CTRL)]            = REG_FRE_DET_CTRL_RST;
    s->regs[REG_INDEX(REG_FRE_UP_LIM)]              = REG_FRE_UP_LIM_RST;
    s->regs[REG_INDEX(REG_FRE_DOWN_LIM)]            = REG_FRE_DOWN_LIM_RST;
    s->regs[REG_INDEX(REG_24M_27M_CLK_OUTPUT)]      = REG_24M_27M_CLK_OUTPUT_RST;
}

static void allwinner_h616_ccu_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    AwH616ClockCtlState *s = AW_H616_CCU(obj);

    /* Memory mapping */
    memory_region_init_io(&s->iomem, OBJECT(s), &allwinner_h616_ccu_ops, s,
                          TYPE_AW_H616_CCU, AW_H616_CCU_IOSIZE);
    sysbus_init_mmio(sbd, &s->iomem);
}

static const VMStateDescription allwinner_h616_ccu_vmstate = {
    .name = "allwinner-h616-ccu",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, AwH616ClockCtlState, AW_H616_CCU_REGS_NUM),
        VMSTATE_END_OF_LIST()
    }
};

static void allwinner_h616_ccu_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, allwinner_h616_ccu_reset);
    dc->vmsd = &allwinner_h616_ccu_vmstate;
}

static const TypeInfo allwinner_h616_ccu_info = {
    .name          = TYPE_AW_H616_CCU,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_init = allwinner_h616_ccu_init,
    .instance_size = sizeof(AwH616ClockCtlState),
    .class_init    = allwinner_h616_ccu_class_init,
};

static void allwinner_h616_ccu_register(void)
{
    type_register_static(&allwinner_h616_ccu_info);
}

type_init(allwinner_h616_ccu_register)
