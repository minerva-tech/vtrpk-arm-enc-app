/*
 * TI DaVinci DM365 EVM board support
 *
 * Copyright (C) 2011 Texas Instruments Incorporated
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/i2c/at24.h>
#include <linux/i2c/tsc2004.h>
#include <linux/leds.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/slab.h>
#include <linux/mtd/nand.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/spi/eeprom.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <mach/gpio.h>
#include <mach/mux.h>
#include <mach/dm365.h>
#include <mach/common.h>
#include <mach/i2c.h>
#include <mach/serial.h>
#include <mach/mmc.h>
#include <mach/nand.h>
#include <mach/gpio.h>
#include <mach/cputype.h>
#include <mach/keyscan.h>

#include <media/tvp514x.h>
#include <media/tvp7002.h>
#include <media/davinci/videohd.h>

#include "board_dm365_evm_resources.h"

static struct gpio_led dm365evm_leds[] = {
    {
        .name               = "led1",
        .active_low         = 1,
        .default_trigger    = "heartbeat",
        .gpio               = 80,
    },
    {
        .name               = "txd",
        .active_low         = 1,
        .default_state      = 0,
        .default_trigger    = "gpio",
        .gpio               = 82,
    },
    {
        .name               = "error",
        .active_low         = 1,
        .default_state      = 0,
        .default_trigger    = "gpio",
        .gpio               = 87,
    },
    {
        .name               = "rxd",
        .active_low         = 1,
        .default_state      = 0,
        .default_trigger    = "gpio",
        .gpio               = 91,
    },
};

static const struct gpio_led_platform_data dm365evm_led_data = {
    .num_leds   = ARRAY_SIZE(dm365evm_leds),
    .leds       = dm365evm_leds,
};

static struct platform_device *dm365evm_led_dev;

static void
dm365evm_led_setup (void)
{
    int status;

    davinci_cfg_reg(DM365_GPIO80);
    davinci_cfg_reg(DM365_GPIO82);
    davinci_cfg_reg(DM365_PWM1);
    davinci_cfg_reg(DM365_PWM2_G87);

    dm365evm_led_dev = platform_device_alloc("leds-gpio", 0);
    platform_device_add_data(dm365evm_led_dev, &dm365evm_led_data, sizeof(dm365evm_led_data));

    status = platform_device_add(dm365evm_led_dev);
    if (status < 0) {
        platform_device_put(dm365evm_led_dev);
        dm365evm_led_dev = NULL;
    }
}

/* NOTE:  this is geared for the standard config, with a socketed
 * 2 GByte Micron NAND (MT29F16G08FAA) using 128KB sectors.  If you
 * swap chips, maybe with a different block size, partitioning may
 * need to be changed.
 */
/*define NAND_BLOCK_SIZE		SZ_128K*/

/* For Samsung 4K NAND (K9KAG08U0M) with 256K sectors */
/*#define NAND_BLOCK_SIZE		SZ_256K*/

/* For Micron 4K NAND with 512K sectors */
#define NAND_BLOCK_SIZE		SZ_512K

static struct mtd_partition davinci_nand_partitions[] = {
	{
		/* UBL (a few copies) plus U-Boot */
		.name		= "bootloader",
		.offset		= 0,
		.size		= 24 * NAND_BLOCK_SIZE,
		.mask_flags	= MTD_WRITEABLE, /* force read-only */
	}, {
		.name		= "kernel",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_32M,
		.mask_flags	= 0,
	}, {
		.name		= "filesystem",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
		.mask_flags	= 0,
	}
	/* two blocks with bad block table (and mirror) at the end */
};

static struct davinci_nand_pdata davinci_nand_data = {
	.mask_chipsel		= BIT(14),
	.parts			= davinci_nand_partitions,
	.nr_parts		= ARRAY_SIZE(davinci_nand_partitions),
	.ecc_mode		= NAND_ECC_HW,
	.options		= NAND_USE_FLASH_BBT,
	.ecc_bits		= 4,
};

static struct resource davinci_nand_resources[] = {
	{
		.start		= DM365_ASYNC_EMIF_DATA_CE0_BASE,
		.end		= DM365_ASYNC_EMIF_DATA_CE0_BASE + SZ_32M - 1,
		.flags		= IORESOURCE_MEM,
	}, {
		.start		= DM365_ASYNC_EMIF_CONTROL_BASE,
		.end		= DM365_ASYNC_EMIF_CONTROL_BASE + SZ_4K - 1,
		.flags		= IORESOURCE_MEM,
	},
};

static struct platform_device davinci_nand_device = {
	.name			= "davinci_nand",
	.id			= 0,
	.num_resources		= ARRAY_SIZE(davinci_nand_resources),
	.resource		= davinci_nand_resources,
	.dev			= {
		.platform_data	= &davinci_nand_data,
	},
};

static struct i2c_board_info i2c_info[] = {
    {},
};

static struct davinci_i2c_platform_data i2c_pdata = {
	.bus_freq	= 400	/* kHz */,
	.bus_delay	= 0	/* usec */,
};

static int cpld_mmc_get_ro(int module)
{
	return 0;
}

static struct davinci_mmc_config dm365evm_mmc_config = {
	.get_ro		= cpld_mmc_get_ro,
	.wires		= 4,
	.max_freq	= 50000000,
	.caps		= MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED,
	.version	= MMC_CTLR_VERSION_2,
};

#if 0
static struct tvp514x_platform_data tvp5146_pdata = {
	.clk_polarity = 0,
	.hs_polarity = 1,
	.vs_polarity = 1
};

#define TVP514X_STD_ALL        (V4L2_STD_NTSC | V4L2_STD_PAL)
/* Inputs available at the TVP5146 */
static struct v4l2_input tvp5146_inputs[] = {
	{
		.index = 0,
		.name = "Composite",
		.type = V4L2_INPUT_TYPE_CAMERA,
		.std = TVP514X_STD_ALL,
	},
	{
		.index = 1,
		.name = "S-Video",
		.type = V4L2_INPUT_TYPE_CAMERA,
		.std = TVP514X_STD_ALL,
	},
};

/*
 * this is the route info for connecting each input to decoder
 * ouput that goes to vpfe. There is a one to one correspondence
 * with tvp5146_inputs
 */
static struct vpfe_route tvp5146_routes[] = {
	{
		.input = INPUT_CVBS_VI2B,
		.output = OUTPUT_10BIT_422_EMBEDDED_SYNC,
	},
{
		.input = INPUT_SVIDEO_VI2C_VI1C,
		.output = OUTPUT_10BIT_422_EMBEDDED_SYNC,
	},
};

static struct vpfe_subdev_info vpfe_sub_devs[] = {
	{
		.module_name = TVP514X_MODULE_NAME,
		.grp_id = VPFE_SUBDEV_TVP5146,
		.num_inputs = ARRAY_SIZE(tvp5146_inputs),
		.inputs = tvp5146_inputs,
		.routes = tvp5146_routes,
		.can_route = 1,
		.ccdc_if_params = {
			.if_type = V4L2_MBUS_FMT_YUYV8_2X8,
			.hdpol = VPFE_PINPOL_POSITIVE,
			.vdpol = VPFE_PINPOL_POSITIVE,
		},
		.board_info = {
			I2C_BOARD_INFO("tvp5146", 0x5d),
			.platform_data = &tvp5146_pdata,
		},
	},
};

#else

static struct v4l2_input teplovisor_inputs[] = {
	{
		.index = 0,
		.name = "FPGA",
		.type = V4L2_INPUT_TYPE_CAMERA,
        .std = V4L2_STD_NTSC,
	},
};

static struct vpfe_subdev_info vpfe_sub_devs[] = {
    {
		.module_name = "teplovisor",
        .is_platform = 1,
		.grp_id = VPFE_SUBDEV_TEPLOVISOR,
		.num_inputs = ARRAY_SIZE(teplovisor_inputs),
		.inputs = teplovisor_inputs,
		.ccdc_if_params = {
			.if_type = V4L2_MBUS_FMT_YUYV8_2X8,
			.hdpol = VPFE_PINPOL_POSITIVE,
			.vdpol = VPFE_PINPOL_POSITIVE,
		},
        .pdev = {
            .name = "teplovisor",
            .id = 0,
        },
	},
};

#endif

 /* Set the input mux for TVP7002/TVP5146/MTxxxx sensors */
static int dm365evm_setup_video_input(enum vpfe_subdev_id id)
{
	const char *label;

	switch (id) {
	case VPFE_SUBDEV_TVP5146:
		label = "tvp5146 SD";
		break;
	case VPFE_SUBDEV_MT9P031:
		label = "HD imager";
		break;
	case VPFE_SUBDEV_TVP7002:
		label = "tvp7002 HD";
		break;
	default:
		return 0;
	}

	pr_info("EVM: switch to %s video input\n", label);
	return 0;
}

static struct vpfe_config vpfe_cfg = {
	.setup_input = dm365evm_setup_video_input,
	.num_subdevs = ARRAY_SIZE(vpfe_sub_devs),
	.sub_devs = vpfe_sub_devs,
	.card_name = "DM365 EVM",
	.num_clocks = 1,
	.clocks = {"vpss_master"},
};

static void dm365evm_usb_configure(void)
{
	davinci_cfg_reg(DM365_GPIO33);
	gpio_request(33, "usb");
	gpio_direction_output(33, 1);
    davinci_cfg_reg(DM365_SPI1_SCLK);
	davinci_setup_usb(500, 8);
}

static void __init evm_init_i2c(void)
{
	davinci_init_i2c(&i2c_pdata);
	i2c_register_board_info(1, i2c_info, ARRAY_SIZE(i2c_info));
}

static struct platform_device *dm365_evm_nand_devices[] __initdata = {
	&davinci_nand_device,
};

static struct resource dm365_evm_fpga_resources[] = {
    {
        .start  = DM365_ASYNC_EMIF_DATA_CE1_BASE,
        .end    = DM365_ASYNC_EMIF_DATA_CE1_BASE + SECTION_SIZE - 1,
        .flags  = IORESOURCE_MEM,
    },
    {
        .start  = DM365_ASYNC_EMIF_CONTROL_BASE,
        .end    = DM365_ASYNC_EMIF_CONTROL_BASE + SZ_4K - 1,
        .flags  = IORESOURCE_MEM,
    },
};

static struct platform_device dm365_evm_fpga_device = {
    .name           = "teplovisor-fpga",
    .id             = 0,
    .num_resources  = ARRAY_SIZE(dm365_evm_fpga_resources),
    .resource       = dm365_evm_fpga_resources,
    .dev            = {
        .platform_data  = NULL,
    },
};

void enable_lcd(void)
{
	return;
}
EXPORT_SYMBOL(enable_lcd);

void enable_hd_clk(void)
{
	return;
}
EXPORT_SYMBOL(enable_hd_clk);

static struct davinci_uart_config uart_config __initdata = {
    .enabled_uarts = (1 << 1) | (1 << 0),
};

static void __init dm365_evm_map_io(void)
{
	/* setup input configuration for VPFE input devices */
	dm365_set_vpfe_config(&vpfe_cfg);
	/* setup configuration for vpbe devices */
	//dm365_set_vpbe_display_config(&vpbe_display_cfg);
	dm365_init();
}

static struct spi_eeprom microchip_25lc1024 = {
	.byte_len   = 1024 * SZ_1K / 8,
	.name       = "25LC1024",
	.page_size  = 256,
	.flags      = EE_ADDR3,
};

static struct spi_board_info dm365_evm_spi_info __initconst = {
    .modalias       = "at25",
    .platform_data  = &microchip_25lc1024,
    .max_speed_hz   = 20 * 1000 * 1000,
    .bus_num        = 4,
    .chip_select    = 0,
    .mode           = SPI_MODE_0,
};

static struct dm365_spi_unit_desc dm365_evm_spi_udesc = {
    .spi_hwunit = 4,
    .chipsel    = BIT(0),
    .irq        = IRQ_DM365_SPIINT3_0,
    .dma_tx_chan    = DMA_CHAN_SPI0_TX,
    .dma_rx_chan    = DMA_CHAN_SPI0_RX,
    .dma_evtq   = DMA_EVQ_SPI0,
    .pdata      = {
        .version    = SPI_VERSION_1,
        .num_chipselect = 5,
        .clk_internal   = 1,
        .cs_hold        = 1,
        .intr_level     = 0,
        .poll_mode      = 1, /* 0 -> interrupt mode 1-> polling mode */
        .use_dma        = 0, /* when 1, value in poll_mode is ignored */
        .c2tdelay       = 0,
        .t2cdelay       = 0
    }
};

static __init void dm365_evm_init(void)
{
    int ret;

    evm_init_i2c();

    davinci_cfg_reg(DM365_UART0_RXD);
    davinci_cfg_reg(DM365_UART0_TXD);
    davinci_cfg_reg(DM365_UART1_RXD);
    davinci_cfg_reg(DM365_UART1_TXD);
    davinci_cfg_reg(DM365_UART1_RTS);
    davinci_cfg_reg(DM365_UART1_CTS);
    davinci_cfg_reg(DM365_EMAC_TXD3);
    davinci_cfg_reg(DM365_EMAC_TXD1);
    
    davinci_serial_init(&uart_config);

    davinci_cfg_reg(DM365_EMAC_RXD3); /* gio10 */

    dm365evm_led_setup();

    davinci_setup_mmc(0, &dm365evm_mmc_config);

    platform_add_devices(dm365_evm_nand_devices, ARRAY_SIZE(dm365_evm_nand_devices));

    platform_device_register(&dm365_evm_fpga_device);

#ifdef CONFIG_SND_DM365_AIC3X_CODEC
	dm365_init_asp(&dm365_evm_snd_data);
#elif defined(CONFIG_SND_DM365_VOICE_CODEC)
	dm365_init_vc(&dm365_evm_snd_data);
#endif
	//dm365_init_rtc();
	//dm365_init_ks(&dm365evm_ks_data);

	dm365_init_spi(&dm365_evm_spi_udesc, 1, &dm365_evm_spi_info);

	//dm365_init_tsc2004();
	dm365evm_usb_configure();
}

MACHINE_START(DAVINCI_DM365_EVM, "DaVinci DM36x EVM")
	.boot_params	= (0x80000100),
	.map_io		= dm365_evm_map_io,
	.init_irq	= davinci_irq_init,
	.timer		= &davinci_timer,
	.init_machine	= dm365_evm_init,
MACHINE_END

