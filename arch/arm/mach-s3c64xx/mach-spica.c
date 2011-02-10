/* linux/arch/arm/mach-s3c6410/mach-spica.c
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/leds.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/smsc911x.h>
#include <linux/regulator/fixed.h>
#include <linux/android_pmem.h>
#include <linux/reboot.h>

#include <video/platform_lcd.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/hardware.h>
#include <mach/regs-fb.h>
#include <mach/map.h>

#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/cpu-single.h>

#include <plat/regs-serial.h>
#include <mach/regs-modem.h>
#include <mach/regs-gpio.h>
#include <mach/regs-sys.h>
#include <mach/regs-srom.h>
#include <plat/iic.h>
#include <plat/fb.h>
#include <mach/gpio-cfg.h>

#include <mach/s3c6410.h>
#include <mach/spica.h>
#include <mach/spica_mem.h>
#include <mach/spica_param.h>
#include <mach/regs-gpio-memport.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <mach/system.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/adc.h>
#include <plat/ts.h>

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

struct device *switch_dev;
EXPORT_SYMBOL(switch_dev);

void (*sec_set_param_value)(int idx, void *value);
EXPORT_SYMBOL(sec_set_param_value);

void (*sec_get_param_value)(int idx, void *value);
EXPORT_SYMBOL(sec_get_param_value);

extern void (*ftm_enable_usb_sw)(int mode);
extern void fsa9480_SetAutoSWMode(void);
extern void fsa9480_MakeRxdLow(void);

#define UCON S3C2410_UCON_DEFAULT
#define ULCON S3C2410_LCON_CS8 | S3C2410_LCON_PNONE
#define UFCON S3C2410_UFCON_RXTRIG8 | S3C2410_UFCON_FIFOMODE

#define S3C64XX_KERNEL_PANIC_DUMP_SIZE 0x8000 /* 32kbytes */
void *S3C64XX_KERNEL_PANIC_DUMP_ADDR;

static struct s3c2410_uartcfg spica_uartcfgs[] __initdata = {
	[0] = {	/* Phone */
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = UCON,
		.ulcon	     = ULCON,
		.ufcon	     = UFCON,
	},
	[1] = {	/* Bluetooth */
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = UCON,
		.ulcon	     = ULCON,
		.ufcon	     = UFCON,
	},
	[2] = {	/* Serial */
		.hwport	     = 2,
		.flags	     = 0,
		.ucon	     = UCON,
		.ulcon	     = ULCON,
		.ufcon	     = UFCON,
	},
};

#if defined(CONFIG_I2C_GPIO)
static struct i2c_gpio_platform_data i2c3_platdata = {
	.sda_pin		= GPIO_PWR_I2C_SDA,
	.scl_pin		= GPIO_PWR_I2C_SCL,
	.udelay			= 2,	/* 250KHz */		
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 1,
};

static struct platform_device s3c_device_i2c3 = {
	.name				= "i2c-gpio",
	.id					= 3,
	.dev.platform_data	= &i2c3_platdata,
};

static struct i2c_gpio_platform_data i2c5_platdata = {
	.sda_pin		= GPIO_FM_I2C_SDA,
	.scl_pin		= GPIO_FM_I2C_SCL,
	.udelay			= 2,	/* 250KHz */		
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 1,
};

static struct platform_device s3c_device_i2c5 = {
	.name				= "i2c-gpio",
	.id					= 5,
	.dev.platform_data	= &i2c5_platdata,
};

static struct i2c_gpio_platform_data i2c6_platdata = {
	.sda_pin		= GPIO_TOUCH_I2C_SDA,
	.scl_pin		= GPIO_TOUCH_I2C_SCL,
	.udelay			= 6,	/* KSS : Change I2C speed to avoid error in TSP */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
//	.scl_is_output_only	= 1,
};

static struct platform_device s3c_device_i2c6 = {
	.name				= "i2c-gpio",
	.id					= 6,
	.dev.platform_data	= &i2c6_platdata,
};
#endif

#ifdef CONFIG_FB_S3C64XX_S6D04D1
struct platform_device sec_device_backlight = {
	.name   = "s6d04d1-backlight",
	.id     = -1,
};
#elif defined(CONFIG_FB_S3C64XX_S6D05A)
struct platform_device sec_device_backlight = {
	.name   = "s6d05a-backlight",
	.id     = -1,
};
#endif

struct platform_device sec_device_dpram = {
	.name	= "dpram-device",
	.id		= -1,
};

struct platform_device sec_device_ts = {
	.name	= "qt5480-ts",
	.id		= -1,
};

struct platform_device sec_device_eled = {
	.name   = "bd2802gue2-eled",
	.id		= -1,

};

struct platform_device sec_device_battery = {
	.name   = "spica-battery",
	.id		= -1,
};

struct platform_device sec_device_rfkill = {
	.name = "bt_rfkill",
	.id = -1,
};

struct platform_device sec_device_btsleep = {
	.name = "bt_sleep",
	.id = -1,
};

#ifdef CONFIG_SEC_HEADSET
static struct sec_headset_port sec_headset_port[] = {
        {
		{ // HEADSET detect info
			.eint		= IRQ_EINT(10), 
			.gpio		= GPIO_DET_35,   
			.gpio_af	= GPIO_DET_35_AF  , 
			.low_active 	= 0
		},{ // SEND/END info
			.eint		= IRQ_EINT(11), 
			.gpio		= GPIO_EAR_SEND_END, 
			.gpio_af	= GPIO_EAR_SEND_END_AF, 
			.low_active	= 1
		}
        }
};
 
static struct sec_headset_platform_data sec_headset_data = {
	.port           = sec_headset_port,
	.nheadsets      = ARRAY_SIZE(sec_headset_port),
};

static struct platform_device sec_device_headset = {
	.name	= "sec_headset",
        .id	= -1,
        .dev	= {
		.platform_data	= &sec_headset_data,
        },
};
#endif

#ifdef CONFIG_ANDROID_PMEM
static struct android_pmem_platform_data pmem_pdata = {
	.name		= "pmem",
	.no_allocator	= 1,
	.cached		= 1,
	.buffered	= 1,	//09.12.01 hoony: surfaceflinger optimize
	.start		= RESERVED_PMEM_START,
	.size		= RESERVED_PMEM,
};
 
static struct android_pmem_platform_data pmem_gpu1_pdata = {
	.name		= "pmem_gpu1",
//	.no_allocator	= 1,
	.cached		= 1,
	.buffered	= 1,
        .start		= GPU1_RESERVED_PMEM_START,
        .size		= RESERVED_PMEM_GPU1,
};

static struct android_pmem_platform_data pmem_render_pdata = {
	.name		= "pmem_render",
	.no_allocator	= 1,
	.cached		= 0,
        .start		= RENDER_RESERVED_PMEM_START,
        .size		= RESERVED_PMEM_RENDER,
};

static struct android_pmem_platform_data pmem_stream_pdata = {
	.name		= "pmem_stream",
	.no_allocator	= 1,
	.cached		= 0,
	.start		= STREAM_RESERVED_PMEM_START,
	.size		= RESERVED_PMEM_STREAM,
};

static struct android_pmem_platform_data pmem_preview_pdata = {
	.name		= "pmem_preview",
	.no_allocator	= 1,
	.cached		= 0,
        .start		= PREVIEW_RESERVED_PMEM_START,
        .size		= RESERVED_PMEM_PREVIEW,
};

static struct android_pmem_platform_data pmem_picture_pdata = {
	.name		= "pmem_picture",
	.no_allocator	= 1,
	.cached		= 0,
        .start		= PICTURE_RESERVED_PMEM_START,
        .size		= RESERVED_PMEM_PICTURE,
};

static struct android_pmem_platform_data pmem_jpeg_pdata = {
	.name		= "pmem_jpeg",
	.no_allocator	= 1,
	.cached		= 0,
        .start		= JPEG_RESERVED_PMEM_START,
        .size		= RESERVED_PMEM_JPEG,
};

static struct android_pmem_platform_data pmem_skia_pdata = {
	.name		= "pmem_skia",
	.no_allocator	= 1,
	.cached		= 0,
        .start		= SKIA_RESERVED_PMEM_START,
        .size		= RESERVED_PMEM_SKIA,
};
 
static struct platform_device pmem_device = {
	.name		= "android_pmem",
	.id		= 0,
	.dev		= { .platform_data = &pmem_pdata },
};
 
static struct platform_device pmem_gpu1_device = {
	.name		= "android_pmem",
	.id		= 1,
	.dev		= { .platform_data = &pmem_gpu1_pdata },
};

static struct platform_device pmem_render_device = {
	.name		= "android_pmem",
	.id		= 2,
	.dev		= { .platform_data = &pmem_render_pdata },
};

static struct platform_device pmem_stream_device = {
	.name		= "android_pmem",
	.id		= 3,
	.dev		= { .platform_data = &pmem_stream_pdata },
};

static struct platform_device pmem_preview_device = {
	.name		= "android_pmem",
	.id		= 5,
	.dev		= { .platform_data = &pmem_preview_pdata },
};

static struct platform_device pmem_picture_device = {
	.name		= "android_pmem",
	.id		= 6,
	.dev		= { .platform_data = &pmem_picture_pdata },
};

static struct platform_device pmem_jpeg_device = {
	.name		= "android_pmem",
	.id		= 7,
	.dev		= { .platform_data = &pmem_jpeg_pdata },
};

static struct platform_device pmem_skia_device = {
	.name		= "android_pmem",
	.id		= 8,
	.dev		= { .platform_data = &pmem_skia_pdata },
};

static struct platform_device *pmem_devices[] = {
	&pmem_device,
	&pmem_gpu1_device,
	&pmem_render_device,
	&pmem_stream_device,
	&pmem_preview_device,
	&pmem_picture_device,
	&pmem_jpeg_device,
	&pmem_skia_device
};

static void __init spica_add_mem_devices(void)
{
	unsigned i;
	for (i = 0; i < ARRAY_SIZE(pmem_devices); ++i)
		if (pmem_devices[i]->dev.platform_data) {
			struct android_pmem_platform_data *pmem = 
					pmem_devices[i]->dev.platform_data;

			if (pmem->size)
				platform_device_register(pmem_devices[i]);
		}
}
#endif

#ifdef CONFIG_S3C64XX_ADCTS
static struct s3c_adcts_plat_info s3c_adcts_cfgs __initdata = {
	.channel = {
		{ /* 0 */
			.delay = 0xFF,
			.presc = 49,
			.resol = S3C_ADCCON_RESSEL_12BIT,
		},{ /* 1 */
			.delay = 0xFF,
			.presc = 49,
			.resol = S3C_ADCCON_RESSEL_12BIT,
		},{ /* 2 */
			.delay = 0xFF,
			.presc = 49,
			.resol = S3C_ADCCON_RESSEL_12BIT,
		},{ /* 3 */
			.delay = 0xFF,
			.presc = 49,
			.resol = S3C_ADCCON_RESSEL_12BIT,
		},{ /* 4 */
			.delay = 0xFF,
			.presc = 49,
			.resol = S3C_ADCCON_RESSEL_12BIT,
		},{ /* 5 */
			.delay = 0xFF,
			.presc = 49,
			.resol = S3C_ADCCON_RESSEL_12BIT,
		},{ /* 6 */
			.delay = 0xFF,
			.presc = 49,
			.resol = S3C_ADCCON_RESSEL_12BIT,
		},{ /* 7 */
			.delay = 0xFF,
			.presc = 49,
			.resol = S3C_ADCCON_RESSEL_12BIT,
		},
	},
};
#endif

static struct s3c_fb_pd_win spica_fb_win0 = {
	.win_mode	= {
		.pixclock	= 1000000000000ULL /
				((10+10+10+320)*(3+2+8+480)*60),
		.left_margin	= 10,
		.right_margin	= 10,
		.upper_margin	= 3,
		.lower_margin	= 8,
		.hsync_len	= 10,
		.vsync_len	= 2,
		.xres		= 320,
		.yres		= 480,
	},
	.max_bpp	= 32,
	.default_bpp	= 16,
};

static struct s3c_fb_platdata spica_lcd_pdata __initdata = {
	.setup_gpio	= s3c64xx_fb_gpio_setup_24bpp,
	.win[0]		= &spica_fb_win0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
	.vidcon1	= VIDCON1_INV_HSYNC | VIDCON1_INV_VSYNC |
			  VIDCON1_INV_VCLK,
};

#define S3C64XX_IODESC(x) { \
	(unsigned long)S3C64XX_VA_##x, \
	__phys_to_pfn(S3C64XX_PA_##x), \
	S3C64XX_SZ_##x, MT_DEVICE\
	}

static struct map_desc spica_iodesc[] __initdata = {
	S3C64XX_IODESC(FB),
	S3C64XX_IODESC(SROM),
	S3C64XX_IODESC(HOSTIFA),
	S3C64XX_IODESC(HOSTIFB),
	S3C64XX_IODESC(ONENAND0),
};

/* LCD Controller */
static struct resource s3c_lcd_resource[] = {
	[0] = {
		.start = S3C64XX_PA_FB,
		.end   = S3C64XX_PA_FB + SZ_1M - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_LCD_VSYNC,
		.end   = IRQ_LCD_SYSTEM,
		.flags = IORESOURCE_IRQ,
	}
};

static u64 s3c_device_lcd_dmamask = 0xffffffffUL;

struct platform_device s3c_device_lcd = {
	.name		  = "s3c-lcd",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3c_lcd_resource),
	.resource	  = s3c_lcd_resource,
	.dev              = {
		.dma_mask		= &s3c_device_lcd_dmamask,
		.coherent_dma_mask	= 0xffffffffUL
	}
};

/* Platform devices */
static struct platform_device *spica_devices[] __initdata = {
#if defined(CONFIG_S3C_DMA_PL080_SOL)
	&s3c_device_dma0,
	&s3c_device_dma1,
	&s3c_device_dma2,
	&s3c_device_dma3,
#endif
//	&s3c_device_keypad,
	&s3c_device_hsmmc0,
	&s3c_device_hsmmc2,
	&s3c_device_rtc,
	&s3c_device_i2c0,
	&s3c_device_i2c1,
#if defined(CONFIG_I2C_GPIO)
	&s3c_device_i2c3,
	&s3c_device_i2c5,
	&s3c_device_i2c6,
	&s3c_device_fb,
#endif
//	&s3c_device_spi0,
//	&sec_device_ts,
#ifdef CONFIG_S3C64XX_ADCTS
//	&s3c_device_adcts,
#endif
#ifdef CONFIG_S3C_ADC
//	&s3c_device_adc,
#endif
	&s3c_device_lcd,
	&s3c_device_usb_hsotg,
#if 0
	&s3c_device_camif,
	&s3c_device_mfc,
	&s3c_device_g3d,
	&s3c_device_2d,
	&s3c_device_rotator,
	&s3c_device_jpeg,
	&s3c_device_vpp,
	&sec_device_backlight, 
#endif
	&sec_device_dpram,
	&sec_device_eled,
	&sec_device_battery,
	&sec_device_rfkill,   //BT POWER ON-OFF
	&sec_device_btsleep,  //BT SLEEP-AWAKE
#if 0
	&sec_device_headset,
#endif
};

static struct i2c_board_info i2c_devs0[] __initdata = {
};

static struct i2c_board_info i2c_devs1[] __initdata = {
};

#if 0 /*def CONFIG_S3C_ADC*/
static struct s3c_adc_mach_info s3c_adc_platform __initdata = {
	/* Support 12-bit resolution */
	.delay		= 0xff,
	.presc 		= 49,
	.resolution	= 12,
};
#endif

static void __init spica_fixup(struct machine_desc *desc,
		struct tag *tags, char **cmdline, struct meminfo *mi)
{
	mi->nr_banks = 1;
	
	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].size = PHYS_UNRESERVED_SIZE;
	mi->bank[0].node = 0;
}

static void __init s3c64xx_allocate_memory_regions(void)
{
#if 0
	void *addr;
	unsigned long size;

	size = S3C64XX_KERNEL_PANIC_DUMP_SIZE;
	addr = alloc_bootmem(size);
	S3C64XX_KERNEL_PANIC_DUMP_ADDR = addr;
#endif
}

static void __init spica_map_io(void)
{
	s3c64xx_init_io(spica_iodesc, ARRAY_SIZE(spica_iodesc));
	s3c24xx_init_clocks(12000000);
	s3c24xx_init_uarts(spica_uartcfgs, ARRAY_SIZE(spica_uartcfgs));
//	smartq_lcd_mode_set();

//	s3c64xx_gpiolib_init();
}

static void spica_set_qos(void) 
{
#if 0
	u32 reg;     							 /* AXI sfr */     

	reg = (u32) ioremap((unsigned long) S3C6410_PA_AXI_SYS, SZ_4K); /* QoS override: FIMD min. latency */
	writel(0xffb6, S3C_VA_SYS + 0x128);  	    			/* AXI QoS */
	writel(0x7, reg + 0x460);   					/* (8 - MFC ch.) */
	writel(0x7ff7, reg + 0x464);      				/* Bus cacheable */
	writel(0x8ff, S3C_VA_SYS + 0x838); 
	
	__raw_writel(0x0, S3C_AHB_CON0);
	
	iounmap(reg);
#endif
} 

/*
 *	Power Off Handler
 */

extern int get_usb_cable_state(void);

#define AV					(0x1 << 14)
#define TTY					(0x1 << 13)
#define PPD					(0x1 << 12)
#define JIG_UART_OFF		(0x1 << 11)
#define JIG_UART_ON			(0x1 << 10)
#define JIG_USB_OFF			(0x1 << 9)
#define JIG_USB_ON			(0x1 << 8)
#define USB_OTG				(0x1 << 7)
#define DEDICATED_CHARGER	(0x1 << 6)
#define USB_CHARGER			(0x1 << 5)
#define CAR_KIT				(0x1 << 4)
#define UART				(0x1 << 3)
#define USB					(0x1 << 2)
#define AUDIO_TYPE2			(0x1 << 1)
#define AUDIO_TYPE1			(0x1 << 0)

//extern void arch_reset(char mode);
#if 0
static void spica_pm_power_off(void)
{
	int	mode = REBOOT_MODE_NONE;
	char reset_mode = 'r';

	if (!gpio_get_value(GPIO_TA_CONNECTED_N)) {	/* Reboot Charging */
		mode = REBOOT_MODE_CHARGING;
		if (sec_set_param_value)
			sec_set_param_value(__REBOOT_MODE, &mode);
		/* Watchdog Reset */
		arch_reset(reset_mode, 0);
	}
	else {	/* Power Off or Reboot */
		if (sec_set_param_value)
			sec_set_param_value(__REBOOT_MODE, &mode);

#if 0
		if (get_usb_cable_state() & (JIG_UART_ON | JIG_UART_OFF | JIG_USB_OFF | JIG_USB_ON)) {
			/* Watchdog Reset */
			arch_reset(reset_mode, 0);
		}
		else
#endif
		{
			/* POWER_N -> Input */
			gpio_direction_input(GPIO_POWER_N);
			/* VREG_MSMP_26V -> Input */
			//gpio_direction_input(GPIO_VREG_MSMP_26V);
			/* Check Power Off Condition */
			//if (!gpio_get_value(GPIO_POWER_N)) {// || gpio_get_value(GPIO_VREG_MSMP_26V)) {
				while (!gpio_get_value(GPIO_POWER_N));  /* Wait Power Button Release */
#if 0
				/* Wait Phone Power Off */	
				while (gpio_get_value(GPIO_VREG_MSMP_26V)); 
#endif	
			//}

			if (!gpio_get_value(GPIO_TA_CONNECTED_N) || !gpio_get_value(GPIO_TA_CHG_N) ) {
				mode = REBOOT_MODE_CHARGING;
				if (sec_set_param_value)
					sec_set_param_value(__REBOOT_MODE, &mode);
				/* Watchdog Reset */
				arch_reset(reset_mode, 0);			
			}
			else
			{
				msleep(50);
				if (sec_set_param_value)
					sec_set_param_value(__REBOOT_MODE, &mode);				
				/* PS_HOLD -> Output Low */
				gpio_direction_output(GPIO_PDA_PS_HOLD, GPIO_LEVEL_HIGH);
				s3c_gpio_setpull(GPIO_PDA_PS_HOLD, S3C_GPIO_PULL_NONE);
				gpio_set_value(GPIO_PDA_PS_HOLD, GPIO_LEVEL_LOW);
			}
		}
	}

	while (1);
}
#endif

#if 0
static void spica_ftm_enable_usb_sw(int mode)
{
	pr_info("%s: mode(%d)\n", __func__, mode);
	if (mode) {
		fsa9480_SetAutoSWMode();
	} else {
		fsa9480_MakeRxdLow();
		mdelay(10);
		fsa9480_MakeRxdLow();
	}
}
#endif

static int uart_current_owner = 0;

static ssize_t uart_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{	
	if ( uart_current_owner )		
		return sprintf(buf, "%s[UART Switch] Current UART owner = PDA \n", buf);	
	else			
		return sprintf(buf, "%s[UART Switch] Current UART owner = MODEM \n", buf);
}

static ssize_t uart_switch_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t size)
{	
	int switch_sel;

	if (sec_get_param_value)
		sec_get_param_value(__SWITCH_SEL, &switch_sel);

	if (strncmp(buf, "PDA", 3) == 0 || strncmp(buf, "pda", 3) == 0)	{		
		gpio_set_value(GPIO_UART_SEL, GPIO_LEVEL_HIGH);		
		uart_current_owner = 1;		
		switch_sel |= UART_SEL_MASK;
		printk("[UART Switch] Path : PDA\n");	
	}	

	if (strncmp(buf, "MODEM", 5) == 0 || strncmp(buf, "modem", 5) == 0) {		
		gpio_set_value(GPIO_UART_SEL, GPIO_LEVEL_LOW);		
		uart_current_owner = 0;		
		switch_sel &= ~UART_SEL_MASK;
		printk("[UART Switch] Path : MODEM\n");	
	}	

	if (sec_set_param_value)
		sec_set_param_value(__SWITCH_SEL, &switch_sel);

	return size;
}

static DEVICE_ATTR(uart_sel, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, uart_switch_show, uart_switch_store);

static int spica_notifier_call(struct notifier_block *this, unsigned long code, void *_cmd)
{
	int	mode = REBOOT_MODE_NONE;

	if ((code == SYS_RESTART) && _cmd) {
		if (!strcmp((char *)_cmd, "arm11_fota"))
			mode = REBOOT_MODE_ARM11_FOTA;
		else if (!strcmp((char *)_cmd, "arm9_fota"))
			mode = REBOOT_MODE_ARM9_FOTA;
		else if (!strcmp((char *)_cmd, "recovery")) 
			mode = REBOOT_MODE_RECOVERY;
		else if (!strcmp((char *)_cmd, "download")) 
			mode = REBOOT_MODE_DOWNLOAD;
	}

	if (sec_set_param_value)
		sec_set_param_value(__REBOOT_MODE, &mode);
	
	return NOTIFY_DONE;
}

static struct notifier_block spica_reboot_notifier = {
	.notifier_call = spica_notifier_call,
};

static void spica_switch_init(void)
{
	sec_class = class_create(THIS_MODULE, "sec");
	if (IS_ERR(sec_class))
		pr_err("Failed to create class(sec)!\n");

	switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");
	if (IS_ERR(switch_dev))
		pr_err("Failed to create device(switch)!\n");

	if (gpio_is_valid(GPIO_UART_SEL)) {
		if (gpio_request(GPIO_UART_SEL, "UART_SEL")) 
			printk(KERN_ERR "Failed to request GPIO for UART_SEL!\n");
		gpio_direction_output(GPIO_UART_SEL, gpio_get_value(GPIO_UART_SEL));
	}
	s3c_gpio_setpull(GPIO_UART_SEL, S3C_GPIO_PULL_NONE);

	if (device_create_file(switch_dev, &dev_attr_uart_sel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_uart_sel.attr.name);
};

#if 0/* defined(CONFIG_RTC_DRV_S3C)*/
/* RTC common Function for samsung APs*/
unsigned int s3c_rtc_set_bit_byte(void __iomem *base, uint offset, uint val)
{
	writeb(val, base + offset);

	return 0;
}

unsigned int s3c_rtc_read_alarm_status(void __iomem *base)
{
	return 1;
}

void s3c_rtc_set_pie(void __iomem *base, uint to)
{
	unsigned int tmp;

	tmp = readw(base + S3C_RTCCON) & ~S3C_RTCCON_TICEN;

        if (to)
                tmp |= S3C_RTCCON_TICEN;

        writew(tmp, base + S3C_RTCCON);
}

void s3c_rtc_set_freq_regs(void __iomem *base, uint freq, uint s3c_freq)
{
	unsigned int tmp;

        tmp = readw(base + S3C_RTCCON) & (S3C_RTCCON_TICEN | S3C_RTCCON_RTCEN );
        writew(tmp, base + S3C_RTCCON);
        s3c_freq = freq;
        tmp = (32768 / freq)-1;
        writel(tmp, base + S3C_TICNT);
}

void s3c_rtc_enable_set(struct platform_device *pdev,void __iomem *base, int en)
{
	unsigned int tmp;

	if (!en) {
		tmp = readw(base + S3C_RTCCON);
		writew(tmp & ~ (S3C_RTCCON_RTCEN | S3C_RTCCON_TICEN), base + S3C_RTCCON);
	} else {
		/* re-enable the device, and check it is ok */
		if ((readw(base+S3C_RTCCON) & S3C_RTCCON_RTCEN) == 0){
			dev_info(&pdev->dev, "rtc disabled, re-enabling\n");

			tmp = readw(base + S3C_RTCCON);
			writew(tmp|S3C_RTCCON_RTCEN, base+S3C_RTCCON);
		}

		if ((readw(base + S3C_RTCCON) & S3C_RTCCON_CNTSEL)){
			dev_info(&pdev->dev, "removing RTCCON_CNTSEL\n");

			tmp = readw(base + S3C_RTCCON);
			writew(tmp& ~S3C_RTCCON_CNTSEL, base+S3C_RTCCON);
		}

		if ((readw(base + S3C_RTCCON) & S3C_RTCCON_CLKRST)){
			dev_info(&pdev->dev, "removing RTCCON_CLKRST\n");

			tmp = readw(base + S3C_RTCCON);
			writew(tmp & ~S3C_RTCCON_CLKRST, base+S3C_RTCCON);
		}
	}
}
#endif

#if defined(CONFIG_KEYPAD_S3C) || defined (CONFIG_KEYPAD_S3C_MODULE)
void s3c_setup_keypad_cfg_gpio(int rows, int columns)
{
	unsigned int gpio;
	unsigned int end;

	end = S3C64XX_GPK(8 + rows);

	/* Set all the necessary GPK pins to special-function 0 */
	for (gpio = S3C64XX_GPK(8); gpio < end; gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	}

	end = S3C64XX_GPL(0 + columns);

	/* Set all the necessary GPK pins to special-function 0 */
	for (gpio = S3C64XX_GPL(0); gpio < end; gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	}
}

EXPORT_SYMBOL(s3c_setup_keypad_cfg_gpio);
#endif

void s3c_setup_uart_cfg_gpio(unsigned char port)
{
	if (port == 0) {
		s3c_gpio_cfgpin(GPIO_FLM_RXD, S3C_GPIO_SFN(GPIO_FLM_RXD_AF));
		s3c_gpio_setpull(GPIO_FLM_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_FLM_TXD, S3C_GPIO_SFN(GPIO_FLM_TXD_AF));
		s3c_gpio_setpull(GPIO_FLM_TXD, S3C_GPIO_PULL_NONE);
	}
	else if (port == 1) {
		s3c_gpio_cfgpin(GPIO_BT_RXD, S3C_GPIO_SFN(GPIO_BT_RXD_AF));
		s3c_gpio_setpull(GPIO_BT_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_TXD, S3C_GPIO_SFN(GPIO_BT_TXD_AF));
		s3c_gpio_setpull(GPIO_BT_TXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_CTS, S3C_GPIO_SFN(GPIO_BT_CTS_AF));
		s3c_gpio_setpull(GPIO_BT_CTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_RTS, S3C_GPIO_SFN(GPIO_BT_RTS_AF));
		s3c_gpio_setpull(GPIO_BT_RTS, S3C_GPIO_PULL_NONE);
	}
	else if (port == 2) {
		s3c_gpio_cfgpin(GPIO_PDA_RXD, S3C_GPIO_SFN(GPIO_PDA_RXD_AF));
		s3c_gpio_setpull(GPIO_PDA_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_PDA_TXD, S3C_GPIO_SFN(GPIO_PDA_TXD_AF));
		s3c_gpio_setpull(GPIO_PDA_TXD, S3C_GPIO_PULL_NONE);
	}
}
EXPORT_SYMBOL(s3c_setup_uart_cfg_gpio);

void s3c_reset_uart_cfg_gpio(unsigned char port)
{
#if 0  // dgahn.temp
	if (port == 0) {
		s3c_gpio_cfgpin(GPIO_FLM_RXD, S3C_GPIO_SFN(GPIO_FLM_RXD_AF));
		s3c_gpio_setpull(GPIO_FLM_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_FLM_TXD, S3C_GPIO_SFN(GPIO_FLM_TXD_AF));
		s3c_gpio_setpull(GPIO_FLM_TXD, S3C_GPIO_PULL_NONE);
	}
#endif
	if (port == 1) {
		s3c_gpio_cfgpin(GPIO_BT_RXD, 0);
		s3c_gpio_setpull(GPIO_BT_RXD, S3C_GPIO_PULL_DOWN);
		s3c_gpio_cfgpin(GPIO_BT_TXD, 0);
		s3c_gpio_setpull(GPIO_BT_TXD, S3C_GPIO_PULL_DOWN);
		s3c_gpio_cfgpin(GPIO_BT_CTS, 0);
		s3c_gpio_setpull(GPIO_BT_CTS, S3C_GPIO_PULL_DOWN);
		s3c_gpio_cfgpin(GPIO_BT_RTS, 0);
		s3c_gpio_setpull(GPIO_BT_RTS, S3C_GPIO_PULL_DOWN);
	}
	else if (port == 2) {
		s3c_gpio_cfgpin(GPIO_PDA_RXD, S3C_GPIO_SFN(GPIO_PDA_RXD_AF));
		s3c_gpio_setpull(GPIO_PDA_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_PDA_TXD, S3C_GPIO_SFN(GPIO_PDA_TXD_AF));
		s3c_gpio_setpull(GPIO_PDA_TXD, S3C_GPIO_PULL_NONE);
	}
}
EXPORT_SYMBOL(s3c_reset_uart_cfg_gpio);

static struct s3c_gpio_config spica_gpio_table[] = {
	/*
	 * OFF PART
	 */

	/* GPA */
	{ GPIO_FLM_RXD, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_FLM_TXD, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_MSENSE_RST, 1, GPIO_LEVEL_HIGH, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT1, S3C_GPIO_PULL_NONE }, 
	{ GPIO_BT_RXD, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_BT_TXD, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_BT_CTS, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_BT_RTS, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },

	/* GPB */
	{ GPIO_PDA_RXD, GPIO_PDA_RXD_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_PDA_TXD, GPIO_PDA_TXD_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
#if (CONFIG_SPICA_REV >= CONFIG_SPICA_TEST_REV02)
	{ GPIO_I2C1_SCL, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_I2C1_SDA, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
#else
	{ GPIO_I2C1_SCL, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },
	{ GPIO_I2C1_SDA, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },
#endif
	{ GPIO_TOUCH_EN, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT1, S3C_GPIO_PULL_NONE },
	{ GPIO_I2C0_SCL, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },
	{ GPIO_I2C0_SDA, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },

	/* GPC */
	{ GPIO_PM_SET1, GPIO_PM_SET1_AF, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },	
	{ GPIO_PM_SET2, GPIO_PM_SET2_AF, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },	
	{ GPIO_PM_SET3, GPIO_PM_SET3_AF, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },	
	{ GPIO_WLAN_CMD, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },	
	{ GPIO_WLAN_CLK, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_WLAN_WAKE, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
	{ GPIO_BT_WAKE, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },

	/* GPD */
	{ GPIO_I2S_CLK, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_BT_WLAN_REG_ON, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
	{ GPIO_I2S_LRCLK, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_I2S_DI, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_I2S_DO, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },

	/* GPE */
	{ GPIO_BT_RST_N, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
	{ GPIO_BOOT, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },
	{ GPIO_WLAN_RST_N, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
	{ GPIO_PWR_I2C_SCL, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },
	{ GPIO_PWR_I2C_SDA, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },

	/* GPF */
	{ GPIO_CAM_MCLK, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_CAM_HSYNC, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_CAM_PCLK, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_MCAM_RST_N, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
	{ GPIO_CAM_VSYNC, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_CAM_D_0, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_CAM_D_1, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_CAM_D_2, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_CAM_D_3, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_CAM_D_4, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_CAM_D_5, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_CAM_D_6, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_CAM_D_7, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_VIBTONE_PWM, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },

	/* GPG */
	{ GPIO_TF_CLK, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_TF_CMD, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },
	{ GPIO_TF_D_0, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },
	{ GPIO_TF_D_1, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },
	{ GPIO_TF_D_2, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },
	{ GPIO_TF_D_3, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },

	/* GPH */
	{ GPIO_TOUCH_I2C_SCL, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },	
	{ GPIO_TOUCH_I2C_SDA, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },
	{ GPIO_FM_I2C_SCL, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },
	{ GPIO_FM_I2C_SDA, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },
	{ GPIO_VIB_EN, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
	{ GPIO_WLAN_D_0, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },
	{ GPIO_WLAN_D_1, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },
	{ GPIO_WLAN_D_2, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },
	{ GPIO_WLAN_D_3, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },

	/* GPI */
	{ GPIO_LCD_B_0, GPIO_LCD_B_0_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_B_1, GPIO_LCD_B_1_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_B_2, GPIO_LCD_B_2_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_B_3, GPIO_LCD_B_3_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_B_4, GPIO_LCD_B_4_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_B_5, GPIO_LCD_B_5_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_G_0, GPIO_LCD_G_0_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_G_1, GPIO_LCD_G_1_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_G_2, GPIO_LCD_G_2_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_G_3, GPIO_LCD_G_3_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_G_4, GPIO_LCD_G_4_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_G_5, GPIO_LCD_G_5_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },

	/* GPJ */
	{ GPIO_LCD_R_0, GPIO_LCD_R_0_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_R_1, GPIO_LCD_R_1_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_R_2, GPIO_LCD_R_2_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_R_3, GPIO_LCD_R_3_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_R_4, GPIO_LCD_R_4_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_R_5, GPIO_LCD_R_5_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_HSYNC, GPIO_LCD_HSYNC_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_VSYNC, GPIO_LCD_VSYNC_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_DE, GPIO_LCD_DE_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },
	{ GPIO_LCD_CLK, GPIO_LCD_CLK_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_DOWN },

	/*
	 * ALIVE PART
	 */

	/* GPK */
	{ GPIO_TA_EN, GPIO_TA_EN_AF, GPIO_LEVEL_HIGH, S3C_GPIO_PULL_NONE, 0, 0 }, 
	{ GPIO_AUDIO_EN, GPIO_AUDIO_EN_AF, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_PHONE_ON, GPIO_PHONE_ON_AF, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_MICBIAS_EN, GPIO_MICBIAS_EN_AF, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_TOUCH_RST, GPIO_TOUCH_RST_AF, GPIO_LEVEL_HIGH, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_CAM_EN, GPIO_CAM_EN_AF, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_PHONE_RST_N, GPIO_PHONE_RST_N_AF, GPIO_LEVEL_HIGH, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_KEYSCAN_0, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_KEYSCAN_1, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_KEYSCAN_2, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_KEYSCAN_3, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ S3C64XX_GPK(12), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ S3C64XX_GPK(13), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ S3C64XX_GPK(14), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_VREG_MSMP_26V, GPIO_VREG_MSMP_26V_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },

	/* GPL */
	{ GPIO_KEYSENSE_0, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_KEYSENSE_1, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_KEYSENSE_2, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_KEYSENSE_3, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_USIM_BOOT, GPIO_USIM_BOOT_AF, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_CAM_3M_STBY_N, GPIO_CAM_3M_STBY_N_AF, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
#if (CONFIG_SPICA_REV >= CONFIG_SPICA_TEST_REV02)
	{ GPIO_HOLD_KEY_N, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
#else
	{ S3C64XX_GPL(9), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
#endif
	{ S3C64XX_GPL(10), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_TA_CONNECTED_N, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_TOUCH_INT_N, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_CP_BOOT_SEL, GPIO_CP_BOOT_SEL_AF, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_BT_HOST_WAKE, GPIO_BT_HOST_WAKE_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, 0, 0 },

	/* GPM */
	{ S3C64XX_GPM(0), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ S3C64XX_GPM(1), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_TA_CHG_N, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_PDA_ACTIVE, GPIO_PDA_ACTIVE_AF, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 }, 
	{ S3C64XX_GPM(4), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ S3C64XX_GPM(5), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },

	/* GPN */
	{ GPIO_ONEDRAM_INT_N, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_WLAN_HOST_WAKE, GPIO_WLAN_HOST_WAKE_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN, 0, 0 },
	{ GPIO_MSENSE_INT, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_ACC_INT, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_SIM_DETECT_N, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_POWER_N, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_TF_DETECT, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_PHONE_ACTIVE, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_PMIC_INT_N, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_JACK_INT_N, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_DET_35, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_EAR_SEND_END, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_RESOUT_N, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_BOOT_EINT13, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_BOOT_EINT14, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_BOOT_EINT15, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },

	/*
	 * MEMORY PART
	 */

	/* GPO */
	{ S3C64XX_GPO(4), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
	{ S3C64XX_GPO(5), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },

	/* GPP */
	{ S3C64XX_GPP(8), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
	{ S3C64XX_GPP(10), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
	{ S3C64XX_GPP(14), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },

	/* GPQ */
	{ S3C64XX_GPQ(2), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
	{ S3C64XX_GPQ(3), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
	{ S3C64XX_GPQ(4), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
	{ S3C64XX_GPQ(5), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
	{ S3C64XX_GPQ(6), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
};

static struct s3c_gpio_config spica_sleep_gpio_table[] = {
	/*
	 * ALIVE PART
	 */

	/* GPK */
	{ GPIO_TOUCH_RST, GPIO_TOUCH_RST_AF, GPIO_LEVEL_HIGH, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_CAM_EN, GPIO_CAM_EN_AF, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_PHONE_RST_N, GPIO_PHONE_RST_N_AF, GPIO_LEVEL_HIGH, S3C_GPIO_PULL_NONE, 0, 0 },
	// [ SEC Kernel2 : Enable Anykey Wakeup
#if 0
	{ GPIO_KEYSCAN_0, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_KEYSCAN_1, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_KEYSCAN_2, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_KEYSCAN_3, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },
#endif
	// ] SEC Kernel2 : Enable Anykey Wakeup
	{ S3C64XX_GPK(12), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ S3C64XX_GPK(13), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ S3C64XX_GPK(14), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_VREG_MSMP_26V, GPIO_VREG_MSMP_26V_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },

	/* GPL */
	{ GPIO_KEYSENSE_0, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_KEYSENSE_1, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_KEYSENSE_2, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_KEYSENSE_3, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ S3C64XX_GPL(4), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ S3C64XX_GPL(5), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ S3C64XX_GPL(6), 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_USIM_BOOT, GPIO_USIM_BOOT_AF, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_CAM_3M_STBY_N, GPIO_CAM_3M_STBY_N_AF, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 },
	{ GPIO_TOUCH_INT_N, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, 0, 0 },

	/* GPM */
	{ GPIO_PDA_ACTIVE, GPIO_PDA_ACTIVE_AF, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE, 0, 0 }, 

	/*
	 * MEMORY PART
	 */

	/* GPO */
	{ GPIO_LCD_RST_N, GPIO_LCD_RST_N_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
	{ GPIO_LCD_CS_N, GPIO_LCD_CS_N_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
	{ GPIO_LCD_SDI, GPIO_LCD_SDI_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
	{ GPIO_LCD_ID, GPIO_LCD_ID_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_INPUT, S3C_GPIO_PULL_NONE },
	{ GPIO_LCD_SCLK, GPIO_LCD_SCLK_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE, S3C_GPIO_SLP_OUT0, S3C_GPIO_PULL_NONE },
};

static void check_pmic(void)
{	
#if 0
	unsigned char reg_buff = 0;
	if (Get_MAX8698_PM_REG(ELDO3, &reg_buff)) {
		pr_info("%s: OTGI 1.2V (%d)\n", __func__, reg_buff);
	}
	if (Get_MAX8698_PM_REG(ELDO4, &reg_buff)) {
		pr_info("%s: VLED 3.3V (%d)\n", __func__, reg_buff);
	}
	if (Get_MAX8698_PM_REG(ELDO5, &reg_buff)) {
		pr_info("%s: VTF 3.0V (%d)\n", __func__, reg_buff);
		if (reg_buff)
			Set_MAX8698_PM_REG(ELDO5, 0);
	}
	if (Get_MAX8698_PM_REG(ELDO6, &reg_buff)) {
		pr_info("%s: VLCD 1.8V (%d)\n", __func__, reg_buff);
	}
	if (Get_MAX8698_PM_REG(ELDO7, &reg_buff)) {
		pr_info("%s: VLCD 3.0V (%d)\n", __func__, reg_buff);
	}
	if (Get_MAX8698_PM_REG(ELDO8, &reg_buff)) {
		pr_info("%s: OTG 3.3V (%d)\n", __func__, reg_buff);
	}
#endif
}

void s3c_config_sleep_gpio(void)
{	
	int spcon_val;

	check_pmic();
	s3c_init_gpio(spica_sleep_gpio_table,
					ARRAY_SIZE(spica_sleep_gpio_table));

	spcon_val = __raw_readl(S3C64XX_SPCON);
	spcon_val = spcon_val & (~0xFFEC0000);
	__raw_writel(spcon_val, S3C64XX_SPCON);
	__raw_writel(0x20, S3C64XX_SPCONSLP);

	/* mem interface reg config in sleep mode */
	__raw_writel(0x00005000, S3C64XX_MEM0CONSLP0);
	__raw_writel(0x01041595, S3C64XX_MEM0CONSLP1);
	__raw_writel(0x10055000, S3C64XX_MEM1CONSLP);

}
EXPORT_SYMBOL(s3c_config_sleep_gpio);

void s3c_config_wakeup_gpio(void)
{
#if 0
	unsigned char reg_buff = 0;
	if (Get_MAX8698_PM_REG(ELDO5, &reg_buff)) {
		pr_info("%s: VTF 3.0V (%d)\n", __func__, reg_buff);
		if (!reg_buff)
			Set_MAX8698_PM_REG(ELDO5, 1);
	}
#endif
}
EXPORT_SYMBOL(s3c_config_wakeup_gpio);

void s3c_config_wakeup_source(void)
{
	unsigned int eint0pend_val;

	/* Wake-up source 
	 * ONEDRAM_INT(EINT0), Power key(EINT5), WLAN_HOST_WAKE(EINT1), 
	 * DET_3.5(EINT10), EAR_SEND_END(EINT11), SIM_nDETECT(EINT4), T_FLASH_DETECT(EINT6)
	 * Hold key(EINT17), TA_CONNECTED(EINT19),
	 * BT_HOST_WAKE(EINT22), CHG_ING(EINT25)
	 * T_FLASH_DETECT(EINT6), 
	 */

	//SEC_BP_WONSUK_20090811
	//register INTB(EINT9) with wakeup source 
#if 0
	eint0pend_val= __raw_readl(S3C64XX_EINT0PEND);
	eint0pend_val |= (0x1 << 25) | (0x1 << 22) | (0x1 << 19) |
		(0x1 << 17) | (0x1 << 11) | (0x1 << 10) | (0x1 << 9)| (0x1 << 6) |(0x1 << 5) | (0x1 << 4) |(0x1 << 1) | 0x1;
	__raw_writel(eint0pend_val, S3C64XX_EINT0PEND);

	eint0pend_val = (0x1 << 25) | (0x1 << 22) | (0x1 << 19) |
		(0x1 << 17) | (0x1 << 11) | (0x1 << 10) |  (0x1 << 9)| (0x1 << 6) |(0x1 << 5) | (0x1 << 4) | (0x1 << 1) | 0x1;
	__raw_writel(~eint0pend_val, S3C64XX_EINT0MASK);

#else	// WLAN_HOST_WAKE(EINT1) Wake-up Source disable temporary by hskang.
	eint0pend_val= __raw_readl(S3C64XX_EINT0PEND);
	eint0pend_val |= (0x1 << 25) | (0x1 << 22) | (0x1 << 19) |
		(0x1 << 17) | (0x1 << 11) | (0x1 << 10) | (0x1 << 9)| (0x1 << 6) |(0x1 << 5) | (0x1 << 4) | (0x1 << 1) | 0x1;
	__raw_writel(eint0pend_val, S3C64XX_EINT0PEND);

	eint0pend_val = (0x1 << 25) | (0x1 << 22) | (0x1 << 19) |
		(0x1 << 17) | (0x1 << 11) | (0x1 << 10) |  (0x1 << 9)| (0x1 << 6) |(0x1 << 5) | (0x1 << 4) |(0x1 << 1) | 0x1;
	__raw_writel(~eint0pend_val, S3C64XX_EINT0MASK);
#endif
	__raw_writel((0x0FFFFFFF & ~eint0pend_val), S3C_EINT_MASK);	

	/* Alarm Wakeup Enable */
	__raw_writel((__raw_readl(S3C_PWR_CFG) & ~(0x1 << 10)), S3C_PWR_CFG);
}
EXPORT_SYMBOL(s3c_config_wakeup_source);

static void __init spica_machine_init(void)
{
	s3c_init_gpio(spica_gpio_table, ARRAY_SIZE(spica_gpio_table));

	s3c_i2c0_set_platdata(NULL);
	s3c_i2c1_set_platdata(NULL);

#ifdef CONFIG_S3C64XX_ADCTS
//	s3c_adcts_set_platdata (&s3c_adcts_cfgs);
#endif
#if 0/*def CONFIG_S3C_ADC*/
//	s3c_adc_set_platdata(&s3c_adc_platform);
#endif

	s3c_fb_set_platdata(&spica_lcd_pdata);

	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));

	platform_add_devices(spica_devices, ARRAY_SIZE(spica_devices));
#ifdef CONFIG_ANDROID_PMEM
	spica_add_mem_devices();
#endif
	//s3c6410_pm_init();

	//spica_set_qos();

	//pm_power_off = spica_pm_power_off;

	register_reboot_notifier(&spica_reboot_notifier);

	spica_switch_init();

	//ftm_enable_usb_sw = spica_ftm_enable_usb_sw;
}

#define MACH_TYPE_SPICA	MACH_TYPE_SMDK6410
MACHINE_START(SPICA, "SPICA")
	/* Maintainer: Ben Dooks <ben@fluff.org> */
	.phys_io	= S3C_PA_UART & 0xfff00000,
	.io_pg_offst	= (((u32)S3C_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S3C64XX_PA_SDRAM + 0x100,

	.init_irq	= s3c6410_init_irq,
	.fixup		= spica_fixup,
	.map_io		= spica_map_io,
	.init_machine	= spica_machine_init,
	.timer		= &s3c24xx_timer,
MACHINE_END
