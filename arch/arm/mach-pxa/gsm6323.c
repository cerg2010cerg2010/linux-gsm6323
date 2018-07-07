/*
 * Support for GSM6323 board PDAs:
 * RoverPC E5 and Amoi E860
 *
 * Copyright (c) 2018 Sergey Larin
 *
 * Based on em-x270.c, and others.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/mtd/physmap.h>
#include <linux/pda_power.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/usb/gpio_vbus.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
#	if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 29)
#		include <mach/i2c.h>
#	else
#		include <plat/i2c.h>
#	endif
#		include <linux/i2c-pxa.h>
#else
#	include <linux/platform_data/i2c-pxa.h>
#endif
#include <linux/leds.h>
#include <linux/mfd/da903x.h>
#include <linux/power_supply.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 32)
#	include <mach/regs-ssp.h>
#	include <mach/ssp.h>
#elif LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
#	include <plat/ssp.h>
#else
#	include <linux/pxa2xx_ssp.h>
#endif
#include <linux/wm97xx.h>
#include <linux/interrupt.h> // new
#include <linux/irq.h>
#include <linux/apm-emulation.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
#	include <mach/audio.h> // new
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 29)
#else
#	include <mach/regs-ost.h>
#endif
#include <mach/regs-lcd.h>
#include <mach/regs-ac97.h>
#include <mach/hardware.h>
#include <mach/audio.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 35)
#	include <asm/system_info.h>
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
#	include <mach/pxa2xx-regs.h> // new?
#	include <mach/mfp-pxa27x.h>
#	include <mach/mmc.h>
#	include <mach/pxafb.h>
#	include <mach/pxa27x_keypad.h>
#else
#	include "pxa27x.h"
#	include "mfp-pxa27x.h"
#	include <linux/platform_data/mmc-pxamci.h>
#	include <linux/platform_data/usb-ohci-pxa27x.h>
#	include <linux/platform_data/keypad-pxa27x.h>
#	include <linux/platform_data/video-pxafb.h>
#endif
#if defined(CONFIG_USB_ANDROID)
#	include <linux/usb/android_composite.h>
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
#	include <mach/udc.h>
#	include <mach/pxa27x-udc.h>
#	include <mach/ohci.h>
#else
#	include <linux/platform_data/usb-ohci-pxa27x.h>
#	include <linux/platform_data/pxa2xx_udc.h>
#	include "pxa27x-udc.h"
#	include "udc.h"
#endif

#include "devices.h"
#include "generic.h"
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
#	include <linux/fb.h>
#	include "../../../drivers/video/pxafb.h"
#else
#	include "../../../drivers/video/fbdev/pxafb.h"
#endif

#include <linux/spi/spi.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
#	include <mach/pxa2xx_spi.h>
#else
#	include <linux/spi/pxa2xx_spi.h>
#endif

#define GPIO99_GSM6323_MMC_DETECT	(99)
#define GPIO41_GSM6323_OTGID		(41)
#define GPIO89_GSM6323_LCD_EN		(89)
#define GSM6323_SSP_LCD_EN 		(3)
#define GPIO19_GSM6323_USB_PULLUP	(19)
//static struct ssp_device *ssp_lcd;

// compatibility defines
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
#	if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 32)
#		define pxa_ssp_write_reg ssp_write_reg
#		define pxa_ssp_read_reg ssp_read_reg
#		define pxa_ssp_request ssp_request
#		define pxa_ssp_free ssp_free
#		define pxa_set_ffuart_info(x)
#		define pxa_set_stuart_info(x)
#		define pxa_set_btuart_info(x)
#	endif
#	define PXA_GPIO_TO_IRQ(x) IRQ_GPIO(x)
#	define irq_set_irq_type set_irq_type
#	define pxa_set_fb_info(n, x) set_pxa_fb_info(x)
#endif

static unsigned long gsm6323_pin_config[] __initdata = {
	/* DA9030 */
	GPIO0_GPIO | WAKEUP_ON_EDGE_FALL,
	/* Power button */
	GPIO1_GPIO | WAKEUP_ON_EDGE_BOTH,
	/* USB Vbus */
	GPIO11_GPIO | WAKEUP_ON_EDGE_BOTH,
	/* Touchscreen */
	GPIO13_GPIO | WAKEUP_ON_EDGE_BOTH,

	/* AC97 */
	GPIO28_AC97_BITCLK,
	GPIO29_AC97_SDATA_IN_0,
	GPIO30_AC97_SDATA_OUT,
	GPIO31_AC97_SYNC,
	GPIO98_AC97_SYSCLK,

	/* LCD */
	GPIO58_LCD_LDD_0,
	GPIO59_LCD_LDD_1,
	GPIO60_LCD_LDD_2,
	GPIO61_LCD_LDD_3,
	GPIO62_LCD_LDD_4,
	GPIO63_LCD_LDD_5,
	GPIO64_LCD_LDD_6,
	GPIO65_LCD_LDD_7,
	GPIO66_LCD_LDD_8,
	GPIO67_LCD_LDD_9,
	GPIO68_LCD_LDD_10,
	GPIO69_LCD_LDD_11,
	GPIO70_LCD_LDD_12,
	GPIO71_LCD_LDD_13,
	GPIO72_LCD_LDD_14,
	GPIO73_LCD_LDD_15,
	GPIO74_LCD_FCLK,
	GPIO75_LCD_LCLK,
	GPIO76_LCD_PCLK,
	GPIO77_LCD_BIAS,

	/* Matrix keypad */
	GPIO101_KP_MKIN_1 | WAKEUP_ON_LEVEL_HIGH,
	GPIO102_KP_MKIN_2 | WAKEUP_ON_LEVEL_HIGH,
	GPIO97_KP_MKIN_3  | WAKEUP_ON_LEVEL_HIGH,
	GPIO103_KP_MKOUT_0,
	GPIO104_KP_MKOUT_1,
	GPIO105_KP_MKOUT_2,

	/* Bluetooth */
	GPIO42_BTUART_RXD,
	GPIO43_BTUART_TXD,
	GPIO44_BTUART_CTS,
	GPIO45_BTUART_RTS,

	/* GSM UART */
	GPIO34_FFUART_RXD,
	GPIO16_FFUART_TXD,
	GPIO83_FFUART_RTS,
	GPIO46_STUART_RXD,
	GPIO47_STUART_TXD,

	/* MMC */
	GPIO32_MMC_CLK,
	GPIO92_MMC_DAT_0,
	GPIO109_MMC_DAT_1,
	GPIO110_MMC_DAT_2,
	GPIO111_MMC_DAT_3,
	GPIO112_MMC_CMD,
	// GPIO99_GSM6323_MMC_DETECT,
	GPIO99_GPIO | WAKEUP_ON_EDGE_BOTH,

	/* I2C */
	GPIO117_I2C_SCL,
	GPIO118_I2C_SDA,

	/* SSP3 connected to LCD */
	GPIO35_SSP3_SFRM,
	GPIO38_SSP3_TXD,
	GPIO40_SSP3_SCLK,
	/* not RXD, but used to enable display */
	(MFP_CFG_OUT(GPIO89, AF0, DRIVE_HIGH)),

	/* Quick Capture Interface */
	GPIO12_CIF_DD_7,
	GPIO17_CIF_DD_6,
	GPIO23_CIF_MCLK,
	GPIO24_CIF_FV,
	GPIO25_CIF_LV,
	GPIO26_CIF_PCLK,
	GPIO27_CIF_DD_0,

	/* SDRAM (?) */
	GPIO33_nCS_5,

	/* USB (OTGID) */
	// GPIO41_USB_P2_7,
	GPIO41_GPIO,
	MFP_CFG_OUT(GPIO19, AF0, DRIVE_LOW),

	/* OS Timer (?) */
	GPIO9_CHOUT_0,
};

/*
 * GPIO Keys
 */

#define INIT_KEY(_code, _gpio, _desc)	\
	{				\
		.code	= KEY_##_code,	\
		.gpio	= _gpio,	\
		.desc	= _desc,	\
		.type	= EV_KEY,	\
		.wakeup	= 1,		\
	}

static struct gpio_keys_button gsm6323_button_table[] = {
	INIT_KEY(END,		1,	"Power button"),
	INIT_KEY(CAMERA,	14,	"Camera button"),
	INIT_KEY(MEDIA,		39,	"Whell press"),
	{
		.code			= KEY_VOLUMEDOWN,
		.gpio			= 93,
		.desc			= "Wheel down",
		.type			= EV_KEY,
		.wakeup			= 1,
		.debounce_interval	= 30,
	}, {
		.code			= KEY_VOLUMEUP,
		.gpio			= 94,
		.desc			= "Wheel up",
		.type			= EV_KEY,
		.wakeup			= 1,
		.debounce_interval	= 30,
	},
};

static struct gpio_keys_platform_data gpio_keys_data = {
	.buttons	= gsm6323_button_table,
	.nbuttons	= ARRAY_SIZE(gsm6323_button_table),
};

static struct platform_device gpio_keys = {
	.name	= "gpio-keys",
	.dev	= {
		.platform_data = &gpio_keys_data,
	},
	.id	= -1,
};

/*
 * Matrix keypad
 */

static unsigned int gsm6323_matrix_keys[] = {
	KEY(1, 0, KEY_SEND), // call
	KEY(1, 2, KEY_LEFT),
	KEY(1, 3, KEY_UP),
	KEY(2, 0, KEY_HOME), // discard
	KEY(2, 2, KEY_RIGHT),
	KEY(2, 3, KEY_DOWN),
	KEY(3, 0, KEY_ENTER),
	KEY(3, 2, KEY_BACK), // don't know what original button should do...
	KEY(3, 3, KEY_MENU),
};

static struct pxa27x_keypad_platform_data gsm6323_keypad_info = {
	.matrix_key_rows = 4,
	.matrix_key_cols = 4,
	.matrix_key_map = gsm6323_matrix_keys,
	.matrix_key_map_size = ARRAY_SIZE(gsm6323_matrix_keys),
};

/*
 * PXAFB LCD - Tianma TM240320LNFWUG
 */

/* LCD power */
static void gsm6323_lcd_write_cmd(struct ssp_device *ssp_lcd, u32 cmd) {
	int tmp = 2;
	do {
		pxa_ssp_write_reg(ssp_lcd, SSDR, cmd);
		while (pxa_ssp_read_reg(ssp_lcd, SSSR) & 0x10)
			;
		--tmp;
	} while (tmp);
}

static void gsm6323_lcd_power(int on, struct fb_var_screeninfo *si) {
	if (on) {
#if 0
		gpio_set_value(GPIO89_GSM6323_LCD_EN, 0);
		udelay(30);
		gpio_set_value(GPIO89_GSM6323_LCD_EN, 1);
#else
		u32 tmp;
		struct ssp_device *ssp_lcd = NULL;
		struct fb_info *info = container_of(si, struct fb_info, var);
		struct pxafb_info *fbi = TO_INF(info, fb);
		unsigned long flags;

		local_irq_save(flags);
		gpio_set_value(GPIO89_GSM6323_LCD_EN, 0);
		udelay(10);
		__raw_writel(0x1FFF, fbi->mmio_base + LCSR);
		__raw_writel(0x3E3F3F3F, fbi->mmio_base + LCSR1);
		gpio_set_value(GPIO89_GSM6323_LCD_EN, 1);
		udelay(20);
		__raw_writel(__raw_readl(fbi->mmio_base + LCCR0) | 1,
				fbi->mmio_base + LCCR0);
#if 1
		ssp_lcd = pxa_ssp_request(GSM6323_SSP_LCD_EN, "SSP3");
		if (ssp_lcd == NULL)
			return;

		pxa_ssp_write_reg(ssp_lcd, SSCR0, 0xC0000F);
		pxa_ssp_write_reg(ssp_lcd, SSCR1, 0x50000018);
		tmp = pxa_ssp_read_reg(ssp_lcd, SSCR0);
		pxa_ssp_write_reg(ssp_lcd, SSCR0, tmp | 0x80);
		while (pxa_ssp_read_reg(ssp_lcd, SSSR) & 0x10)
			;
		gsm6323_lcd_write_cmd(ssp_lcd, 0x250);
		gsm6323_lcd_write_cmd(ssp_lcd, 0x201);
		tmp = pxa_ssp_read_reg(ssp_lcd, SSCR0);
		pxa_ssp_write_reg(ssp_lcd, SSCR0, tmp & ~0x80);
		pxa_ssp_free(ssp_lcd);
#endif
		local_irq_restore(flags);
#endif
	}
}

static struct pxafb_mode_info gsm6323_lcd_modes[] = {
	{
		.pixclock	= 192308,
		.bpp		= 16,
		.xres		= 240,
		.yres		= 320,

		.hsync_len	= 10,
		.vsync_len	= 2,

		.left_margin	= 19,
		.right_margin	= 10,
		.upper_margin	= 2,
		.lower_margin	= 2,

		.sync		= 0,
	},
};

static struct pxafb_mach_info gsm6323_lcd_info = {
	.modes			= gsm6323_lcd_modes,
	.num_modes		= 1,
	.fixed_modes		= 1,
	.lcd_conn		= LCD_COLOR_TFT_16BPP | LCD_ALTERNATE_MAPPING,
	.pxafb_lcd_power	= gsm6323_lcd_power,
};

/*
 * PXA UDC
 */

static struct pxa2xx_udc_mach_info gsm6323_udc_info = {
	.gpio_pullup		= GPIO19_GSM6323_USB_PULLUP,
};

/*
 * USB device VBus detection
 */

struct gpio_vbus_mach_info gsm6323_gpio_vbus_data = {
	.gpio_vbus		= GPIO41_GSM6323_OTGID,
	.gpio_vbus_inverted	= 1,
	.gpio_pullup		= -1,
};

struct platform_device gsm6323_gpio_vbus = {
	.name	="gpio-vbus",
	.id	= -1,
	.dev	= {
		.platform_data = &gsm6323_gpio_vbus_data,
	}
};

/*
 * USB OHCI
 */
static int gsm6323_ohci_init(struct device *dev)
{
	UP2OCR = UP2OCR_HXS | UP2OCR_HXOE | UP2OCR_DPPDE | UP2OCR_DMPDE;
	return 0;
}

static struct pxaohci_platform_data gsm6323_ohci_info = {
	.port_mode	= PMM_NPS_MODE,
	.flags		= ENABLE_PORT1 | ENABLE_PORT2 | OC_MODE_PERPORT,
	.init		= gsm6323_ohci_init,
};

/*
 * DA9030 PMIC
 */

/* Leds, vibra and backlight */
static struct led_info gsm6323_led_info[] = {
	{
		.name = "gsm6323:red",
		.default_trigger = "battery-charging",
	}, {
		.name = "gsm6323:green",
		.default_trigger = "battery-full",
	}, {
		.name = "gsm6323:blue",
		.flags = DA9030_LED_RATE_ON | (3 << 5) /* 2.1s rate */,
	}, {
		.name = "gsm6323:vibra",
		.flags = DA9030_VIBRA_MODE_2P7V | DA9030_VIBRA_FREQ_8HZ,
	}
};

/* Devices */
#define REG_INIT_CONSUMER(_name, _min_uV, _max_uV, _consumer_arr)	\
	{								\
		.constraints = {					\
			.name = _name,					\
			.min_uV = _min_uV,				\
			.max_uV = _max_uV,				\
		},							\
		.num_consumer_supplies = ARRAY_SIZE(_consumer_arr),	\
		.consumer_supplies = _consumer_arr,			\
	}

#define REG_INIT(_name, _min_uV, _max_uV)				\
	{								\
		.constraints = {					\
			.name = _name,					\
			.min_uV = _min_uV,				\
			.max_uV = _max_uV,				\
			.always_on = 1,					\
		},							\
	}

static struct regulator_init_data gsm6323_ldo_init_data[] = {
	REG_INIT("vcc_gps", 1500000, 1500000),
	REG_INIT("vcc_kbd", 2400000, 2400000),
	REG_INIT("vcc_unk1", 1800000, 1800000),
	REG_INIT("vcc_unk2", 2400000, 2400000),
	REG_INIT("vcc_cam", 1800000, 1800000),
	REG_INIT("vcc_bt", 1800000, 1800000),
	REG_INIT("vcc_unk3", 2400000, 2400000),
	REG_INIT("vcc_camgps", 3200000, 3200000),
	//REG_INIT("vcc_lcd", 3200000, 3200000), // cannot be enabled (why?)
};

/*
 * Battery
 */
static void gsm6323_battery_low(void)
{
#if defined(CONFIG_APM_EMULATION)
	apm_queue_event(APM_LOW_BATTERY);
#endif
}

static void gsm6323_battery_critical(void)
{
#if defined(CONFIG_APM_EMULATION)
	apm_queue_event(APM_CRITICAL_SUSPEND);
#endif
}

static struct power_supply_info gsm6323_psy_info = {
	.name = "battery",
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.voltage_max_design = 4200000,
	.voltage_min_design = 3400000,
	.use_for_apm = 1,
};

static struct da9030_battery_info gsm6323_batterty_info = {
	.battery_info = &gsm6323_psy_info,

	.charge_milliamp = 500,
	.charge_millivolt = 4200,

	.vbat_low = 3600,
	.vbat_crit = 3400,
	.vbat_charge_start = 4100,
	.vbat_charge_stop = 4200,
	.vbat_charge_restart = 4000,

	.vcharge_min = 3200,
	.vcharge_max = 5500,

	.tbat_low = 197,
	.tbat_high = 78,
	.tbat_restart = 100,

	.batmon_interval = 0,

	.battery_low = gsm6323_battery_low,
	.battery_critical = gsm6323_battery_critical,
};

#define DA9030_SUBDEV(_name, _id, _pdata)	\
	{					\
		.name = "da903x-" #_name,	\
		.id = DA9030_ID_##_id,		\
		.platform_data = _pdata,	\
	}

#define DA9030_LDO(num, ind) \
	DA9030_SUBDEV(regulator, LDO##num, &gsm6323_ldo_init_data[ind])

static struct da903x_subdev_info gsm6323_da9030_subdevs[] = {
	//DA9030_LDO(1, 0),
	//DA9030_LDO(2, 1),
	DA9030_LDO(5, 2),
	DA9030_LDO(6, 3),
	//DA9030_LDO(7, 4),
	DA9030_LDO(9, 5),
	DA9030_LDO(10,6),
	//DA9030_LDO(11,7),
	//DA9030_LDO(18,8),

	// BUCK (first) is used originally, but only upper bits are written...
	// maybe this is actually BUCK2, but I don't have a datasheet
	// DA9030_SUBDEV(regulator, BUCK2, &buck2_data),

	DA9030_SUBDEV(led, LED_1, &gsm6323_led_info[0]),
	DA9030_SUBDEV(led, LED_2, &gsm6323_led_info[1]),
	DA9030_SUBDEV(led, LED_3, &gsm6323_led_info[2]),
	DA9030_SUBDEV(led, VIBRA, &gsm6323_led_info[3]),
	DA9030_SUBDEV(backlight, WLED, &gsm6323_led_info[0]),
	DA9030_SUBDEV(battery, BAT, &gsm6323_batterty_info),
};

static struct da903x_platform_data gsm6323_da9030_info = {
	.num_subdevs = ARRAY_SIZE(gsm6323_da9030_subdevs),
	.subdevs = gsm6323_da9030_subdevs,
};

static struct i2c_board_info gsm6323_pwr_i2c_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("da9030", 0x49),
		.irq = PXA_GPIO_TO_IRQ(0),
		.platform_data = &gsm6323_da9030_info,
	},
};

/*
 * MMC/SD
 */
// Doesn't work if I put GPIO number in plarform_data...
static int gsm6323_mci_init(struct device *dev,
			    irq_handler_t detect_int,
			    void *data)
{
	int err = request_irq(gpio_to_irq(GPIO99_GSM6323_MMC_DETECT),
			detect_int, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"MMC card detect", data);
	if (err) {
		dev_err(dev, "Cannot request MMC card IRQ: %d\n", err);
		return err;
	}
	return 0;
}
static struct pxamci_platform_data gsm6323_mci_platform_data = {
	.ocr_mask		= MMC_VDD_32_33 | MMC_VDD_33_34,
	.init			= gsm6323_mci_init,
#define DETECT_DELAY 200
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 32)
	.detect_delay_ms	= DETECT_DELAY,
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 29)
	.gpio_card_ro	 	= -1,
	.gpio_power		= -1,
	.gpio_card_detect 	= -1, // GPIO99_GSM6323_MMC_DETECT,
#endif
};

/*
 * DiskOnChip G4 flash
 */

/* The address should be correct, but driver keeps crashing...
 * There is also StrataFlash (64MB I think) chip soldered,
 * but it appears that it's not used, because:
 * - ~60MB Windows Mobile firmware
 * - ~60MB of internal memory
 * - tiny IPL and BDTL bootloaders
 * Everything of this fits into 128MB DOC memory.
 * But the chip is there, on the board... IDK really!
 */
static struct resource gsm6323_docg4_resource = {
	.start	= PXA_CS0_PHYS,
	.end	= PXA_CS0_PHYS + SZ_128K - 1,
	.flags	= IORESOURCE_MEM,
};

static struct platform_device gsm6323_docg4_flash = {
	.name   = "docg4",
	.id     = -1,
	.resource = &gsm6323_docg4_resource,
	.num_resources = 1,
};

/*
 * PXA I2C power controller
 */

static struct i2c_pxa_platform_data gsm6323_i2c_power_info = {
	.fast_mode	= 0,
};

/*
 * WM9713 touchscreen and audio
 */

static struct platform_device gsm6323_codec = {
	.name = "wm9713-codec",
	.id = -1,
};

static struct platform_device gsm6323_pxa_pcm = {
	.name = "pxa2xx-pcm",
	.id = -1,
};

static struct platform_device gsm6323_card = {
	.name = "gsm6323-wm9713",
	.id = -1,
};

static void wm97xx_irq_enable(struct wm97xx *wm, int enable)
{
	if (enable)
		enable_irq(wm->pen_irq);
	else
		disable_irq_nosync(wm->pen_irq);
}

static struct wm97xx_mach_ops gsm6323_wm9713_mach_ops = {
	.irq_enable = wm97xx_irq_enable,
	.irq_gpio = WM97XX_GPIO_2,
};

static int gsm6323_wm97xx_probe(struct platform_device *pdev)
{
	struct wm97xx *wm = platform_get_drvdata(pdev);
	u16 reg;

	wm->pen_irq = gpio_to_irq(13);
	irq_set_irq_type(wm->pen_irq, IRQ_TYPE_EDGE_BOTH);

	// invert PENDOWN polarity
	reg = wm97xx_reg_read(wm, AC97_WM9713_DIG1);
	wm97xx_reg_write(wm, AC97_WM9713_DIG1, reg | WM9713_PDPOL);

	/* codec specific irq config */
	/* use PEN_DOWN GPIO 13 to assert IRQ on GPIO line 2 */
	wm97xx_config_gpio(wm, WM97XX_GPIO_13, WM97XX_GPIO_IN,
			   WM97XX_GPIO_POL_LOW,
			   WM97XX_GPIO_STICKY,
			   WM97XX_GPIO_WAKE);
	wm97xx_config_gpio(wm, WM97XX_GPIO_2, WM97XX_GPIO_OUT,
			   WM97XX_GPIO_POL_LOW,
			   WM97XX_GPIO_NOTSTICKY,
			   WM97XX_GPIO_NOWAKE);

	return wm97xx_register_mach_ops(wm, &gsm6323_wm9713_mach_ops);
}

static int gsm6323_wm97xx_remove(struct platform_device *pdev)
{
	struct wm97xx *wm = platform_get_drvdata(pdev);

	wm97xx_unregister_mach_ops(wm);
	return 0;
}

static struct platform_driver gsm6323_wm97xx_driver = {
	.probe = gsm6323_wm97xx_probe,
	.remove = gsm6323_wm97xx_remove,
	.driver = {
		.name = "wm97xx-touch",
	},
};

/*
 * Android ADB config
 */

#if defined(CONFIG_USB_ANDROID) || defined(CONFIG_USB_ANDROID_MODULE)

static char *usb_functions_default[] = {
	"usb_mass_storage",
};

static char *usb_functions_default_adb[] = {
	"adb",
	"usb_mass_storage",
};

static char *usb_functions_rndis[] = {
	"rndis",
};

static char *usb_functions_rndis_adb[] = {
	"rndis",
	"adb",
};

static char *usb_functions_all[] = {
#ifdef CONFIG_USB_ANDROID_RNDIS
	"rndis",
#endif
	"adb",
	"usb_mass_storage",
#ifdef CONFIG_USB_ANDROID_ACM
	"acm",
#endif
};

static struct android_usb_product usb_products[] = {
	{
		.product_id	= 0x9025,
		.num_functions	= ARRAY_SIZE(usb_functions_default_adb),
		.functions	= usb_functions_default_adb,
	},
	{
		.product_id	= 0x9026,
		.num_functions	= ARRAY_SIZE(usb_functions_default),
		.functions	= usb_functions_default,
	},
	{
		.product_id	= 0xf00e,
		.num_functions	= ARRAY_SIZE(usb_functions_rndis),
		.functions	= usb_functions_rndis,
	},
	{
		.product_id	= 0x9024,
		.num_functions	= ARRAY_SIZE(usb_functions_rndis_adb),
		.functions	= usb_functions_rndis_adb,
	},
};

static struct usb_mass_storage_platform_data mass_storage_pdata = {
	.nluns		= 1,
	.vendor		= "RoverPC",
	.product        = "Mass storage",
	.release	= 0x0100,
};

static struct platform_device gsm6323_android_usb_mass_storage_device = {
	.name	= "usb_mass_storage",
	.id	= -1,
	.dev	= {
		.platform_data = &mass_storage_pdata,
	},
};

static struct usb_ether_platform_data rndis_pdata = {
	.vendorID	= 0x0bb4,
	.vendorDescr	= "RoverPC",
	.ethaddr	= { 0x00, 0x12, 0x40, 0x11, 0xA8, 0x2A },
};

static struct platform_device gsm6323_android_rndis_device = {
	.name	= "rndis",
	.id	= -1,
	.dev	= {
		.platform_data = &rndis_pdata,
	},
};

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id	= 0x0bb4,
	.product_id	= 0x0c01,
	.functions	= usb_functions_all,
	.num_products	= ARRAY_SIZE(usb_products),
	.products	= usb_products,
	.version	= 0x0100,
	.product_name	= "Android Gadget",
	.manufacturer_name	= "RoverPC", // You may set "AMOI"
	.serial_number	= "1234567890ABCDEF",
	.num_functions	= ARRAY_SIZE(usb_functions_all),
};

static struct platform_device gsm6323_android_usb_device = {
	.name	= "android_usb",
	.id	= -1,
	.dev	= {
		.platform_data = &android_usb_pdata,
	},
};

#endif

/*
 * Platform devices
 */

static struct platform_device *devices[] __initdata = {
	&gpio_keys,
	&gsm6323_codec,
	&gsm6323_pxa_pcm,
	&gsm6323_card,
	&gsm6323_docg4_flash,

	/* Seems like OTG is assumed to be always-on, maybe
	 * it needs additional configuration. Anyway, this one should
	 * work, feel free to uncomment (don't forget ohci line below)
	 */
	// &gsm6323_gpio_vbus,
#ifdef CONFIG_USB_ANDROID
	&gsm6323_android_usb_device,
#endif
#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
	&gsm6323_android_usb_mass_storage_device,
#endif
#ifdef CONFIG_USB_ANDROID_RNDIS
	&gsm6323_android_rndis_device,
#endif
};

static void __init gsm6323_init(void)
{
	// UP2OCR = UP2OCR_HXOE;
	pxa2xx_mfp_config(ARRAY_AND_SIZE(gsm6323_pin_config));

	pxa_set_ffuart_info(NULL);
	pxa_set_btuart_info(NULL);
	pxa_set_stuart_info(NULL);

	pxa27x_set_i2c_power_info(&gsm6323_i2c_power_info);
	pxa_set_i2c_info(&gsm6323_i2c_power_info);
	i2c_register_board_info(1,
		ARRAY_AND_SIZE(gsm6323_pwr_i2c_board_info));

	pxa_set_fb_info(NULL, &gsm6323_lcd_info);

	platform_driver_register(&gsm6323_wm97xx_driver);
	platform_add_devices(ARRAY_AND_SIZE(devices));

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 32)
	gsm6323_mci_platform_data.detect_delay
		= msecs_to_jiffies(DETECT_DELAY);
#endif
	pxa_set_keypad_info(&gsm6323_keypad_info);
	pxa_set_mci_info(&gsm6323_mci_platform_data);
	// pxa_set_ohci_info(&gsm6323_ohci_info);
	pxa_set_ac97_info(NULL);
	pxa_set_udc_info(&gsm6323_udc_info);
}

MACHINE_START(GSM6323, "GSM6323")
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 35)
	.map_io		= pxa_map_io,
	.phys_io        = 0x40000000,
	.io_pg_offst    = (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params    = 0xa0000100,
	.timer		= &pxa_timer,
#else
	.atag_offset	= 0x100,
	.map_io		= pxa27x_map_io,
	.nr_irqs	= PXA_NR_IRQS,
	.handle_irq	= pxa27x_handle_irq,
	.init_time	= pxa_timer_init,
	.restart	= pxa_restart,
#endif
	.init_irq	= pxa27x_init_irq,
	.init_machine	= gsm6323_init,
MACHINE_END
