/*
 * drivers/video/s3c64xxfb.c
 *
 * Copyright (C) 2011 Tomasz Figa <tomasz.figa at gmail.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	S3C Frame Buffer Driver
 *	based on skeletonfb.c, sa1100fb.h, s3c2410fb.c
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/earlysuspend.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#include <mach/regs-gpio.h>
#include <mach/regs-lcd.h>
#include <mach/s3c64xxfb.h>

#include "s3c64xxfb.h"

/*
 * Logging
 */

#define DRIVER_NAME "s3c-lcd"
#define ERR(format, args...) printk(KERN_ERR "%s: " format, DRIVER_NAME, ## args)
#define WARNING(format, args...) printk(KERN_WARNING "%s: " format, DRIVER_NAME, ## args)
#define INFO(format, args...) printk(KERN_INFO "%s: " format, DRIVER_NAME, ## args)

/*
 * Globals
 */

static struct s3c_fb_info s3c_fb_info[S3C_FB_NUM];

/*
 * Hardware support
 */

#define S3C_FB_WINCON_8BPP	S3C_WINCONx_BPPMODE_F_8BPP_PAL \
				| S3C_WINCONx_BYTSWP_ENABLE \
				| S3C_WINCONx_BURSTLEN_4WORD

#define S3C_FB_WINCON_16BPP	S3C_WINCONx_BPPMODE_F_16BPP_565 \
				| S3C_WINCONx_HAWSWP_ENABLE

#define S3C_FB_WINCON_24BPP	S3C_WINCONx_BPPMODE_F_24BPP_888

struct s3c_fb_drvdata s3c_fimd = {
// KSS_2009-09-03 : Change LCD Dot Clk
	.vidcon0	= S3C_VIDCON0_INTERLACE_F_PROGRESSIVE
			| S3C_VIDCON0_VIDOUT_RGB_IF
#ifndef S3C_FB_USE_CLK_DIRECTED
			| S3C_VIDCON0_CLKDIR_DIVIDED
#endif
			| S3C_VIDCON0_CLKSEL(1)
			| S3C_VIDCON0_ENVID_ENABLE,
// End of KSS_2009-09-03
	.vidosd1c	= S3C_VIDOSDxC_ALPHA1_B(S3C_FB_MAX_ALPHA_LEVEL)
			| S3C_VIDOSDxC_ALPHA1_G(S3C_FB_MAX_ALPHA_LEVEL)
			| S3C_VIDOSDxC_ALPHA1_R(S3C_FB_MAX_ALPHA_LEVEL),
	.vidosd2c	= S3C_VIDOSDxC_ALPHA1_B(S3C_FB_MAX_ALPHA_LEVEL)
			| S3C_VIDOSDxC_ALPHA1_G(S3C_FB_MAX_ALPHA_LEVEL)
			| S3C_VIDOSDxC_ALPHA1_R(S3C_FB_MAX_ALPHA_LEVEL),
	.vidosd3c	= S3C_VIDOSDxC_ALPHA1_B(S3C_FB_MAX_ALPHA_LEVEL)
			| S3C_VIDOSDxC_ALPHA1_G(S3C_FB_MAX_ALPHA_LEVEL)
			| S3C_VIDOSDxC_ALPHA1_R(S3C_FB_MAX_ALPHA_LEVEL),
	.vidosd4c	= S3C_VIDOSDxC_ALPHA1_B(S3C_FB_MAX_ALPHA_LEVEL)
			| S3C_VIDOSDxC_ALPHA1_G(S3C_FB_MAX_ALPHA_LEVEL)
			| S3C_VIDOSDxC_ALPHA1_R(S3C_FB_MAX_ALPHA_LEVEL),

	.vidintcon0	= S3C_VIDINTCON0_FRAMESEL0_VSYNC
			| S3C_VIDINTCON0_FRAMESEL1_NONE
			| S3C_VIDINTCON0_INTFRMEN_DISABLE
			| S3C_VIDINTCON0_FIFOSEL_WIN0
			| S3C_VIDINTCON0_FIFOLEVEL_25
			| S3C_VIDINTCON0_INTFIFOEN_DISABLE
			| S3C_VIDINTCON0_INTEN_ENABLE,
	.vidintcon1	= 0,

	.xoffset	= 0,
	.yoffset	= 0,

	.w1keycon0	= S3C_WxKEYCON0_KEYBLEN_DISABLE
			| S3C_WxKEYCON0_KEYEN_F_DISABLE
			| S3C_WxKEYCON0_DIRCON_MATCH_FG_IMAGE
			| S3C_WxKEYCON0_COMPKEY(0x0),
	.w1keycon1	= S3C_WxKEYCON1_COLVAL(0xffffff),
	.w2keycon0	= S3C_WxKEYCON0_KEYBLEN_DISABLE
			| S3C_WxKEYCON0_KEYEN_F_DISABLE
			| S3C_WxKEYCON0_DIRCON_MATCH_FG_IMAGE
			| S3C_WxKEYCON0_COMPKEY(0x0),
	.w2keycon1	= S3C_WxKEYCON1_COLVAL(0xffffff),
	.w3keycon0	= S3C_WxKEYCON0_KEYBLEN_DISABLE
			| S3C_WxKEYCON0_KEYEN_F_DISABLE
			| S3C_WxKEYCON0_DIRCON_MATCH_FG_IMAGE
			| S3C_WxKEYCON0_COMPKEY(0x0),
	.w3keycon1	= S3C_WxKEYCON1_COLVAL(0xffffff),
	.w4keycon0	= S3C_WxKEYCON0_KEYBLEN_DISABLE
			| S3C_WxKEYCON0_KEYEN_F_DISABLE
			| S3C_WxKEYCON0_DIRCON_MATCH_FG_IMAGE
			| S3C_WxKEYCON0_COMPKEY(0x0),
	.w4keycon1	= S3C_WxKEYCON1_COLVAL(0xffffff),

	.sync		= 0,
	.cmap_static	= 1,

	.vs_offset	= S3C_FB_DEFAULT_DISPLAY_OFFSET,
};

static void s3cfb_set_fimd_info(s3cfb_pdata *pdata)
{
	if (!pdata->dither) {
		s3c_fimd.dithmode = 0;
	} else {
		u32 dith_en = S3C_DITHMODE_DITHERING_ENABLE;
		u32 dith_pos = 0;
		switch (pdata->dither & 0xF) {
		case 8:
			dith_pos = S3C_DITHMODE_BDITHPOS_8BIT;
			break;
		case 6:
			dith_pos = S3C_DITHMODE_BDITHPOS_6BIT;
			break;
		case 5:
			dith_pos = S3C_DITHMODE_BDITHPOS_5BIT;
			break;
		default:
			dith_en = 0;
		}
		switch (pdata->dither & 0xF0) {
		case 80:
			dith_pos |= S3C_DITHMODE_GDITHPOS_8BIT;
			break;
		case 60:
			dith_pos |= S3C_DITHMODE_GDITHPOS_6BIT;
			break;
		case 50:
			dith_pos |= S3C_DITHMODE_GDITHPOS_5BIT;
			break;
		default:
			dith_en = 0;
		}
		switch (pdata->dither & 0xF00) {
		case 800:
			dith_pos |= S3C_DITHMODE_RDITHPOS_8BIT;
			break;
		case 600:
			dith_pos |= S3C_DITHMODE_RDITHPOS_6BIT;
			break;
		case 500:
			dith_pos |= S3C_DITHMODE_RDITHPOS_5BIT;
			break;
		default:
			dith_en = 0;
		}
		s3c_fimd.dithmode = dith_en | dith_pos;
	}

	s3c_fimd.vidcon1	= pdata->vidcon1;

	s3c_fimd.vidtcon0	= S3C_VIDTCON0_VBPD(pdata->vbp - 1)
				| S3C_VIDTCON0_VFPD(pdata->vfp - 1)
				| S3C_VIDTCON0_VSPW(pdata->vsw - 1);
	s3c_fimd.vidtcon1	= S3C_VIDTCON1_HBPD(pdata->hbp - 1)
				| S3C_VIDTCON1_HFPD(pdata->hfp - 1)
				| S3C_VIDTCON1_HSPW(pdata->hsw - 1);
	s3c_fimd.vidtcon2	= S3C_VIDTCON2_LINEVAL(pdata->yres - 1)
				| S3C_VIDTCON2_HOZVAL(pdata->xres - 1);

	s3c_fimd.vidosd0a	= S3C_VIDOSDxA_OSD_LTX_F(0)
				| S3C_VIDOSDxA_OSD_LTY_F(0);
	s3c_fimd.vidosd0b	= S3C_VIDOSDxB_OSD_RBX_F(pdata->xres - 1)
				| S3C_VIDOSDxB_OSD_RBY_F(pdata->yres - 1);

	s3c_fimd.vidosd1a	= S3C_VIDOSDxA_OSD_LTX_F(0)
				| S3C_VIDOSDxA_OSD_LTY_F(0);
	s3c_fimd.vidosd1b	= S3C_VIDOSDxB_OSD_RBX_F(pdata->xres - 1)
				| S3C_VIDOSDxB_OSD_RBY_F(pdata->yres - 1);

	s3c_fimd.width		= pdata->width;
	s3c_fimd.height 	= pdata->height;
	s3c_fimd.xres 		= pdata->xres;
	s3c_fimd.yres 		= pdata->yres;
	s3c_fimd.xres_virtual	= pdata->xres;
	s3c_fimd.yres_virtual	= pdata->yres * 2;

	s3c_fimd.pixclock	= pdata->refresh_rate
				* (pdata->hfp + pdata->hsw + pdata->hbp + pdata->xres)
				* (pdata->vfp + pdata->vsw + pdata->vbp + pdata->yres);

	s3c_fimd.hsync_len 	= pdata->hsw;
	s3c_fimd.vsync_len 	= pdata->vsw;
	s3c_fimd.left_margin 	= pdata->hfp;
	s3c_fimd.upper_margin 	= pdata->vfp;
	s3c_fimd.right_margin 	= pdata->hbp;
	s3c_fimd.lower_margin 	= pdata->vbp;

	s3c_fimd.logo_base	= pdata->logo_base;
	s3c_fimd.logo_size	= pdata->logo_size;
}

static void s3cfb_write_palette(struct s3c_fb_info *fbi)
{
	unsigned int i;
	unsigned long ent;
	unsigned int win_num = fbi->win_id;

	writel((s3c_fimd.wpalcon | S3C_WPALCON_PALUPDATEEN), S3C_WPALCON);

	for (i = 0; i < 256; i++) {
		if ((ent = fbi->palette_buffer[i]) == S3C_FB_PALETTE_BUFF_CLEAR)
			continue;

		writel(ent, S3C_WIN_PALENTRY(win_num, i));
	}

	writel(s3c_fimd.wpalcon, S3C_WPALCON);
}

static irqreturn_t s3cfb_irq(int irqno, void *param)
{
	/* for clearing the interrupt source */
	writel(readl(S3C_VIDINTCON1), S3C_VIDINTCON1);

	++s3c_fimd.vsync_count;
	wake_up_interruptible(&s3c_fimd.vsync_wait_queue);

	return IRQ_HANDLED;
}

static void s3cfb_setup_window(struct s3c_fb_info *fbi)
{
	struct clk *lcd_clock;
	struct fb_var_screeninfo *var = &fbi->fb.var;
	unsigned long flags = 0, size, offset, line_size;
	int win_num =  fbi->win_id;

	/* Initialise LCD with values from hare */

	line_size = var->xres_virtual * fbi->bytes_per_pixel;
	offset = (var->xres_virtual - var->xres) * fbi->bytes_per_pixel;
	size = var->yres * line_size;

	fbi->fb_start_addr = fbi->map_dma;
	fbi->fb_end_addr = fbi->map_dma + size;

#if defined(S3C_FB_DISPLAY_LOGO)
	s3cfb_display_logo(win_num);
#endif

	writel(fbi->fb_start_addr, S3C_VIDWADD0B0(win_num));
	writel(S3C_VIDWxxADD1_VBASEL_F(fbi->fb_end_addr),
				S3C_VIDWADD1B0(win_num));
	writel(S3C_VIDWxxADD2_OFFSIZE_F(offset)
				| (S3C_VIDWxxADD2_PAGEWIDTH_F(line_size)),
				S3C_VIDWADD2(win_num));

	writel(fbi->wincon, S3C_WINCON(win_num));
	writel(fbi->vidosda, S3C_VIDOSDA(win_num));
	writel(fbi->vidosdb, S3C_VIDOSDB(win_num));

	switch (win_num) {
	case 0:
		writel(fbi->vidosdd, S3C_VIDOSDC(win_num));
		break;
	case 1:
	case 2:
		writel(fbi->vidosdc, S3C_VIDOSDC(win_num));
		writel(fbi->vidosdd, S3C_VIDOSDD(win_num));
		break;
	default:
		writel(fbi->vidosdc, S3C_VIDOSDC(win_num));
	}
 }

void s3cfb_activate_var(struct s3c_fb_info *fbi, struct fb_var_screeninfo *var)
{
	unsigned id = fbi->win_id;
	unsigned enable = fbi->wincon & S3C_WINCONx_ENWIN_F_ENABLE;

	switch (var->bits_per_pixel) {
	case 8:
		fbi->wincon		= S3C_FB_WINCON_8BPP;
		fbi->bpp		= 8;
		fbi->bytes_per_pixel	= 1;
		break;

	case 16:
		fbi->wincon		= S3C_FB_WINCON_16BPP;
		fbi->bpp		= 16;
		fbi->bytes_per_pixel	= 2;
		break;

	case 32:
		fbi->wincon		= S3C_FB_WINCON_24BPP;
		fbi->bpp		= 32;
		fbi->bytes_per_pixel	= 4;
		break;
	}

	if (id != 0)
		fbi->wincon |= S3C_WINCONx_ALPHA_SEL_1;

	fbi->wincon |= enable;

	/* write new registers */
	writel(fbi->wincon, S3C_WINCON(id));
}

static void s3cfb_set_fb_addr(struct s3c_fb_info *fbi)
{
	/* set buffer addresses */
	__raw_writel(0x800, S3C_PRTCON);
	__raw_writel(fbi->fb_start_addr, S3C_VIDWADD0B0(fbi->win_id));
	__raw_writel(fbi->fb_end_addr, S3C_VIDWADD1B0(fbi->win_id));
	__raw_writel(0, S3C_PRTCON);
}

/******************************************************************************/

#ifdef S3C_FB_DISPLAY_LOGO

void s3cfb_display_logo(int win_num)
{
	struct s3c_fb_info *fbi = &s3c_fb_info[0];
	u16 *logo_virt_buf;
#ifdef CONFIG_FB_S3C64XX_BPP_24
	u32 count;
	u32 *scr_virt_buf = (u32 *)fbi->map_cpu;
#endif

	if(win_num != 0)
		return;

	logo_virt_buf = ioremap_nocache(s3c_fimd.logo_base, s3c_fimd.logo_size);

#ifdef CONFIG_FB_S3C64XX_BPP_24
	count = s3c_fmd.logo_size / 2;
	do {
		u16 srcpix = *(logo_virt_buf++);
		u32 dstpix =	((srcpix & 0xF800) << 8) |
				((srcpix & 0x07E0) << 5) |
				((srcpix & 0x001F) << 3);
		*(scr_virt_buf++) = dstpix;
	} while (--count);
#else
	memcpy(fbi->map_cpu, logo_virt_buf, s3c_fimd.logo_size);
#endif

	iounmap(logo_virt_buf);
}

#if S3C_FB_NUM != 1
#include "s3cfb_progress_hvga.h"

static int progress = 0;
static struct timer_list progress_timer;

static void progress_timer_handler(unsigned long data)
{
	struct s3c_fb_info *fbi = &s3c_fb_info[1];
	unsigned short *bar_src, *bar_dst;
	int	i, j, p;

	/* 1 * 12 R5G5B5 BMP (Aligned 4 Bytes) */
	bar_dst = (unsigned short *)(fbi->map_cpu + (((320 * 416) + 41) * 2));
	bar_src = (unsigned short *)(progress_bar + sizeof(progress_bar) - 4);

	for (i = 0; i < 12; i++) {
		for (j = 0; j < 2; j++) {
			p = ((320 * i) + (progress * 2) + j);
			*(bar_dst + p) = (*(bar_src - (i * 2)) | 0x8000);
		}
	}

	progress++;

	if (progress > 118) {
		del_timer(&progress_timer);
	}
	else {
		progress_timer.expires = (get_jiffies_64() + (HZ/15));
		progress_timer.function = progress_timer_handler;
		add_timer(&progress_timer);
	}
}
#endif

static void s3cfb_start_progress(void)
{
#if S3C_FB_NUM != 1
	struct s3c_fb_info *fbi = &s3c_fb_info[1];
	unsigned short *bg_src, *bg_dst;
	int	i, j, p;
	unsigned int new_wincon1;

	memset(fbi->map_cpu, 0x00, s3c_fimd.logo_size);

	/* 320 * 25 R5G5B5 BMP */
	bg_dst = (unsigned short *)(fbi->map_cpu + ((320 * 410) * 2));
	bg_src = (unsigned short *)(progress_bg + sizeof(progress_bg) - 2);

	for (i = 0; i < 25; i++) {
		for (j = 0; j < 320; j++) {
			p = ((320 * i) + j);
			if ((*(bg_src - p) & 0x7FFF) == 0x0000)
				*(bg_dst + p) = (*(bg_src - p) & ~0x8000);
			else
				*(bg_dst + p) = (*(bg_src - p) | 0x8000);
		}
	}

	new_wincon1 = S3C_WINCONx_ENLOCAL_DMA | S3C_WINCONx_BUFSEL_0 | S3C_WINCONx_BUFAUTOEN_DISABLE | \
	           S3C_WINCONx_BITSWP_DISABLE | S3C_WINCONx_BYTSWP_DISABLE | S3C_WINCONx_HAWSWP_ENABLE | \
	           S3C_WINCONx_BURSTLEN_16WORD | S3C_WINCONx_BLD_PIX_PIXEL | S3C_WINCONx_BPPMODE_F_16BPP_A555 | \
	           S3C_WINCONx_ALPHA_SEL_0 | S3C_WINCONx_ENWIN_F_ENABLE,

	writel(new_wincon1, S3C_WINCON1);

	init_timer(&progress_timer);
	progress_timer.expires = (get_jiffies_64() + (HZ/10));
	progress_timer.function = progress_timer_handler;
	add_timer(&progress_timer);
#endif
}

static void s3cfb_stop_progress(void)
{
#if S3C_FB_NUM != 1
	/* Delete the timer and wait for the handler to finish */
	del_timer_sync(&progress_timer);

	/* Restore window 1 */
	writel(s3c_fimd.wincon1,  S3C_WINCON1);
#endif
}

#endif

static int s3cfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	switch(cmd){
	case FBIO_WAITFORVSYNC:
		return s3cfb_wait_for_vsync();
	default:
		return -EINVAL;
	}

	return 0;
}

static int s3cfb_set_gpio(void)
{
	unsigned long val;
#if defined (CONFIG_MACH_SMDK6410)
	int i, err;
#endif
	/* Must be '0' for Normal-path instead of By-pass */
	writel(0x0, S3C_HOSTIFB_MIFPCON);

	/* enable clock to LCD */
	val = readl(S3C_SCLK_GATE);
	val |= S3C_CLKCON_SCLK_LCD;
	writel(val, S3C_SCLK_GATE);

	/* select TFT LCD type (RGB I/F) */
	val = readl(S3C64XX_SPC_BASE);
	val &= ~0x3;
	val |= (1 << 0);
	writel(val, S3C64XX_SPC_BASE);
#if defined (CONFIG_MACH_SMDK6410)
	/* VD */
	for (i = 0; i < 16; i++)
		s3c_gpio_cfgpin(S3C64XX_GPI(i), S3C_GPIO_SFN(2));

	for (i = 0; i < 12; i++)
		s3c_gpio_cfgpin(S3C64XX_GPJ(i), S3C_GPIO_SFN(2));

	/* backlight ON */
	if (gpio_is_valid(S3C64XX_GPF(15))) {
		err = gpio_request(S3C64XX_GPF(15), "GPF");

		if (err) {
			printk(KERN_ERR "failed to request GPF for "
				"lcd backlight control\n");
			return err;
		}

		gpio_direction_output(S3C64XX_GPF(15), 1);
	}

	/* module reset */
	if (gpio_is_valid(S3C64XX_GPN(5))) {
		err = gpio_request(S3C64XX_GPN(5), "GPN");

		if (err) {
			printk(KERN_ERR "failed to request GPN for "
				"lcd reset control\n");
			return err;
		}

		gpio_direction_output(S3C64XX_GPN(5), 1);
	}

	mdelay(100);

	gpio_set_value(S3C64XX_GPN(5), 0);
	mdelay(10);

	gpio_set_value(S3C64XX_GPN(5), 1);
	mdelay(10);

	gpio_free(S3C64XX_GPF(15));
	gpio_free(S3C64XX_GPN(5));
#endif
	return 0;
}

/*
 * Power management
 */

#if defined(CONFIG_PM)

static struct sleep_save s3c_lcd_save[] = {
	SAVE_ITEM(S3C_VIDCON0),
	SAVE_ITEM(S3C_VIDCON1),

	SAVE_ITEM(S3C_VIDTCON0),
	SAVE_ITEM(S3C_VIDTCON1),
	SAVE_ITEM(S3C_VIDTCON2),
	SAVE_ITEM(S3C_VIDTCON3),

	SAVE_ITEM(S3C_WINCON0),
	SAVE_ITEM(S3C_WINCON1),
	SAVE_ITEM(S3C_WINCON2),
	SAVE_ITEM(S3C_WINCON3),
	SAVE_ITEM(S3C_WINCON4),

	SAVE_ITEM(S3C_VIDOSD0A),
	SAVE_ITEM(S3C_VIDOSD0B),
	SAVE_ITEM(S3C_VIDOSD0C),

	SAVE_ITEM(S3C_VIDOSD1A),
	SAVE_ITEM(S3C_VIDOSD1B),
	SAVE_ITEM(S3C_VIDOSD1C),
	SAVE_ITEM(S3C_VIDOSD1D),

	SAVE_ITEM(S3C_VIDOSD2A),
	SAVE_ITEM(S3C_VIDOSD2B),
	SAVE_ITEM(S3C_VIDOSD2C),
	SAVE_ITEM(S3C_VIDOSD2D),

	SAVE_ITEM(S3C_VIDOSD3A),
	SAVE_ITEM(S3C_VIDOSD3B),
	SAVE_ITEM(S3C_VIDOSD3C),

	SAVE_ITEM(S3C_VIDOSD4A),
	SAVE_ITEM(S3C_VIDOSD4B),
	SAVE_ITEM(S3C_VIDOSD4C),

	SAVE_ITEM(S3C_VIDW00ADD0B0),
	SAVE_ITEM(S3C_VIDW00ADD0B1),
	SAVE_ITEM(S3C_VIDW01ADD0B0),
	SAVE_ITEM(S3C_VIDW01ADD0B1),
	SAVE_ITEM(S3C_VIDW02ADD0),
	SAVE_ITEM(S3C_VIDW03ADD0),
	SAVE_ITEM(S3C_VIDW04ADD0),
	SAVE_ITEM(S3C_VIDW00ADD1B0),
	SAVE_ITEM(S3C_VIDW00ADD1B1),
	SAVE_ITEM(S3C_VIDW01ADD1B0),
	SAVE_ITEM(S3C_VIDW01ADD1B1),
	SAVE_ITEM(S3C_VIDW02ADD1),
	SAVE_ITEM(S3C_VIDW03ADD1),
	SAVE_ITEM(S3C_VIDW04ADD1),
	SAVE_ITEM(S3C_VIDW00ADD2),
	SAVE_ITEM(S3C_VIDW01ADD2),
	SAVE_ITEM(S3C_VIDW02ADD2),
	SAVE_ITEM(S3C_VIDW03ADD2),
	SAVE_ITEM(S3C_VIDW04ADD2),

	SAVE_ITEM(S3C_VIDINTCON0),
	SAVE_ITEM(S3C_VIDINTCON1),
	SAVE_ITEM(S3C_W1KEYCON0),
	SAVE_ITEM(S3C_W1KEYCON1),
	SAVE_ITEM(S3C_W2KEYCON0),
	SAVE_ITEM(S3C_W2KEYCON1),

	SAVE_ITEM(S3C_W3KEYCON0),
	SAVE_ITEM(S3C_W3KEYCON1),
	SAVE_ITEM(S3C_W4KEYCON0),
	SAVE_ITEM(S3C_W4KEYCON1),
	SAVE_ITEM(S3C_DITHMODE),

	SAVE_ITEM(S3C_WIN0MAP),
	SAVE_ITEM(S3C_WIN1MAP),
	SAVE_ITEM(S3C_WIN2MAP),
	SAVE_ITEM(S3C_WIN3MAP),
	SAVE_ITEM(S3C_WIN4MAP),
	SAVE_ITEM(S3C_WPALCON),

	SAVE_ITEM(S3C_TRIGCON),
	SAVE_ITEM(S3C_I80IFCONA0),
	SAVE_ITEM(S3C_I80IFCONA1),
	SAVE_ITEM(S3C_I80IFCONB0),
	SAVE_ITEM(S3C_I80IFCONB1),
	SAVE_ITEM(S3C_LDI_CMDCON0),
	SAVE_ITEM(S3C_LDI_CMDCON1),
	SAVE_ITEM(S3C_SIFCCON0),
	SAVE_ITEM(S3C_SIFCCON1),
	SAVE_ITEM(S3C_SIFCCON2),

	SAVE_ITEM(S3C_LDI_CMD0),
	SAVE_ITEM(S3C_LDI_CMD1),
	SAVE_ITEM(S3C_LDI_CMD2),
	SAVE_ITEM(S3C_LDI_CMD3),
	SAVE_ITEM(S3C_LDI_CMD4),
	SAVE_ITEM(S3C_LDI_CMD5),
	SAVE_ITEM(S3C_LDI_CMD6),
	SAVE_ITEM(S3C_LDI_CMD7),
	SAVE_ITEM(S3C_LDI_CMD8),
	SAVE_ITEM(S3C_LDI_CMD9),
	SAVE_ITEM(S3C_LDI_CMD10),
	SAVE_ITEM(S3C_LDI_CMD11),

	SAVE_ITEM(S3C_W2PDATA01),
	SAVE_ITEM(S3C_W2PDATA23),
	SAVE_ITEM(S3C_W2PDATA45),
	SAVE_ITEM(S3C_W2PDATA67),
	SAVE_ITEM(S3C_W2PDATA89),
	SAVE_ITEM(S3C_W2PDATAAB),
	SAVE_ITEM(S3C_W2PDATACD),
	SAVE_ITEM(S3C_W2PDATAEF),
	SAVE_ITEM(S3C_W3PDATA01),
	SAVE_ITEM(S3C_W3PDATA23),
	SAVE_ITEM(S3C_W3PDATA45),
	SAVE_ITEM(S3C_W3PDATA67),
	SAVE_ITEM(S3C_W3PDATA89),
	SAVE_ITEM(S3C_W3PDATAAB),
	SAVE_ITEM(S3C_W3PDATACD),
	SAVE_ITEM(S3C_W3PDATAEF),
	SAVE_ITEM(S3C_W4PDATA01),
	SAVE_ITEM(S3C_W4PDATA23),
};

static int s3cfb_suspend_sub(void)
{
        s3c_pm_do_save(s3c_lcd_save, ARRAY_SIZE(s3c_lcd_save));

	/* TODO: Move this to some better place */

        /* for Idle Current GPIO Setting */
        __raw_writel(0x55555555, S3C64XX_GPICON);
        __raw_writel(0x0, S3C64XX_GPIDAT);
        __raw_writel(0x55555555, S3C64XX_GPIPUD);
        __raw_writel(0x55555555, S3C64XX_GPJCON);
        __raw_writel(0x0, S3C64XX_GPJDAT);
        __raw_writel(0x55555555, S3C64XX_GPJPUD);

	/* TODO: ^^^ */

        /* sleep before disabling the clock, we need to ensure
	 * the LCD DMA engine is not going to get back on the bus
         * before the clock goes off again (bjd) */

        msleep(1);

        clk_disable(s3c_fimd.vidclk);
	clk_disable(s3c_fimd.clk);

#ifdef USE_LCD_DOMAIN_GATING
        s3c_set_normal_cfg(S3C64XX_DOMAIN_F, S3C64XX_LP_MODE, S3C64XX_LCD);
#endif /* USE_LCD_DOMAIN_GATING */

        return 0;
}

static int s3cfb_resume_sub(void)
{
#ifdef USE_LCD_DOMAIN_GATING
        s3c_set_normal_cfg(S3C64XX_DOMAIN_F, S3C64XX_ACTIVE_MODE, S3C64XX_LCD);
        if(s3c_wait_blk_pwr_ready(S3C64XX_BLK_F)) {
                printk(KERN_ERR "[%s] Domain F is not ready\n", __func__);
                return -1;
        }
#endif /* USE_LCD_DOMAIN_GATING */

	clk_enable(s3c_fimd.clk);
        clk_enable(s3c_fimd.vidclk);

        s3c_pm_do_restore(s3c_lcd_save, ARRAY_SIZE(s3c_lcd_save));

        s3cfb_set_gpio();

        s3cfb_start_lcd();

        return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND

static void s3cfb_early_suspend(struct early_suspend *h)
{
	s3cfb_suspend_sub();
}

static void s3cfb_late_resume(struct early_suspend *h)
{
	s3cfb_resume_sub();
}

/*
 *  Suspend
 */
static int s3cfb_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

/*
 *  Resume
 */
static int s3cfb_resume(struct platform_device *dev)
{
	return 0;
}

/*
 * shutdown
 */
void s3cfb_shutdown(struct platform_device *dev)
{

}

#else /* CONFIG_HAS_EARLYSUSPEND */

/*
 *  Suspend
 */
static int s3cfb_suspend(struct platform_device *dev, pm_message_t state)
{
	return s3cfb_suspend_sub();
}

/*
 *  Resume
 */
static int s3cfb_resume(struct platform_device *dev)
{
	return s3cfb_resume_sub();
}

/*
 * shutdown
 */
static void s3cfb_shutdown(struct platform_device *dev)
{

}

#endif	/* CONFIG_HAS_EARLYSUSPEND */

#else /* CONFIG_PM */

static int s3cfb_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int s3cfb_resume(struct platform_device *dev)
{
	return 0;
}

static void s3cfb_shutdown(struct platform_device *dev)
{

}

#endif /* CONFIG_PM */

/*
 * Memory mapping
 */

static int s3cfb_map_video_memory(struct s3c_fb_info *fbi)
{
	fbi->map_size = PAGE_ALIGN(fbi->fb.fix.smem_len);
	fbi->map_cpu = dma_alloc_writecombine(fbi->dev, fbi->map_size,
						&fbi->map_dma, GFP_KERNEL);

	if (!fbi->map_cpu)
		return -ENOMEM;

	INFO("Allocated %d bytes of FB %d at %08x@%p.\n", fbi->map_size,
				fbi->win_id, fbi->map_dma, fbi->map_cpu);

	/* prevent initial garbage on screen */
	memset(fbi->map_cpu, 0x00, fbi->map_size);

	fbi->fb.fix.smem_start = fbi->map_dma;
	fbi->fb.screen_base = fbi->map_cpu;

	return 0;
}

static void s3cfb_unmap_video_memory(struct s3c_fb_info *fbi)
{
	dma_free_writecombine(fbi->dev, fbi->map_size,
				fbi->map_cpu, fbi->map_dma);
}

/*
 * Framebuffer functions
 */

/**
 *	s3cfb_check_var
 *	@var: frame buffer variable screen structure
 *	@info: frame buffer structure that represents a single frame buffer
 *
 *	Get the video params out of 'var'. If a value doesn't fit, round it up,
 *	if it's too big, return -EINVAL.
 */
static int s3cfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	DPRINTK("check_var(var=%p, info=%p)\n", var, info);

	switch (var->bits_per_pixel) {
		case 8:
			var->red = s3c_fb_rgb_8.red;
			var->green = s3c_fb_rgb_8.green;
			var->blue = s3c_fb_rgb_8.blue;
			var->transp = s3c_fb_rgb_8.transp;
			s3c_fimd.bytes_per_pixel = 1;
			break;

		case 16:
			var->red = s3c_fb_rgb_16.red;
			var->green = s3c_fb_rgb_16.green;
			var->blue = s3c_fb_rgb_16.blue;
			var->transp = s3c_fb_rgb_16.transp;
			s3c_fimd.bytes_per_pixel = 2;
			break;

		case 24:
			var->bits_per_pixel = 32;
			/* drop through */
		case 32:
			var->red = s3c_fb_rgb_24.red;
			var->green = s3c_fb_rgb_24.green;
			var->blue = s3c_fb_rgb_24.blue;
			var->transp = s3c_fb_rgb_24.transp;
			s3c_fimd.bytes_per_pixel = 4;
			break;

		default:
			return -EINVAL;
	}

	return 0;
}

/**
 *      s3cfb_set_par - Optional function. Alters the hardware state.
 *      @info: frame buffer structure that represents a single frame buffer
 */
static int s3cfb_set_par(struct fb_info *info)
{
	struct fb_var_screeninfo *var = &info->var;
	struct s3c_fb_info *fbi = (struct s3c_fb_info *) info;

	if (var->bits_per_pixel == 16 || var->bits_per_pixel == 32)
		fbi->fb.fix.visual = FB_VISUAL_TRUECOLOR;
	else
		fbi->fb.fix.visual = FB_VISUAL_PSEUDOCOLOR;

	fbi->fb.fix.line_length = var->xres_virtual * s3c_fimd.bytes_per_pixel;

	/* activate this new configuration */
	s3cfb_activate_var(fbi, var);

	return 0;
}

/**
 *	s3cfb_pan_display
 *	@var: frame buffer variable screen structure
 *	@info: frame buffer structure that represents a single frame buffer
 *
 *	Pan (or wrap, depending on the `vmode' field) the display using the
 *	`xoffset' and `yoffset' fields of the `var' structure.
 *	If the values don't fit, return -EINVAL.
 *
 *	Returns negative errno on error, or zero on success.
 */
static int s3cfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct s3c_fb_info *fbi = (struct s3c_fb_info *)info;

	/*
	 * All values are checked for correctness in fbmem.c,
	 * so let's just apply the new parameters.
	 */

	fbi->start_addr = fbi->map_dma + var->yoffset * fbi->fb.fix.line_length;
	fbi->end_addr = fbi->start_addr + var->yres * fbi->fb.fix.line_length;

	s3cfb_set_fb_addr(fbi);

	return 0;
}

/**
 *      s3cfb_blank
 *	@blank_mode: the blank mode we want.
 *	@info: frame buffer structure that represents a single frame buffer
 *
 *	Blank the screen if blank_mode != 0, else unblank. Return 0 if
 *	blanking succeeded, != 0 if un-/blanking failed due to e.g. a
 *	video mode which doesn't support it. Implements VESA suspend
 *	and powerdown modes on hardware that supports disabling hsync/vsync:
 *	blank_mode == 2: suspend vsync
 *	blank_mode == 3: suspend hsync
 *	blank_mode == 4: powerdown
 *
 *	Returns negative errno on error, or zero on success.
 *
 */
static int s3cfb_blank(int blank_mode, struct fb_info *info)
{
	// Implement proper blanking here (FB side only)

	return 0;
}

/*
 * Helper functions
 */

static void s3cfb_stop_lcd(void)
{
	if (s3c_fimd.vidcon0 & S3C_VIDCON0_ENVID_ENABLE) {
		s3c_fimd.vidcon0 &= ~S3C_VIDCON0_ENVID_F_ENABLE;
		writel(s3c_fimd.vidcon0, S3C_VIDCON0);
	}
}

static void s3cfb_start_lcd(void)
{
	if ((s3c_fimd.vidcon0 & S3C_VIDCON0_ENVID_ENABLE) == 0) {
		s3c_fimd.vidcon0 |=
			S3C_VIDCON0_ENVID_ENABLE | S3C_VIDCON0_ENVID_F_ENABLE;
		writel(s3c_fimd.vidcon0, S3C_VIDCON0);
	}
}

static int s3cfb_wait_for_vsync(void)
{
	int cnt;

	cnt = s3c_fimd.vsync_count;
	wait_event_interruptible_timeout(s3c_fimd.vsync_wait_queue,
				cnt != s3c_fimd.vsync_count, HZ / 10);

  	return cnt;
}

static void s3cfb_update_palette(struct s3c_fb_info *fbi, unsigned int regno, unsigned int val)
{
	unsigned long flags;

	local_irq_save(flags);

	fbi->palette_buffer[regno] = val;

	if (!fbi->palette_ready) {
		fbi->palette_ready = 1;
		s3c_fimd.palette_win = fbi->win_id;
	}

	local_irq_restore(flags);
}

static inline unsigned int s3cfb_chan_to_field(unsigned int chan, struct fb_bitfield bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf.length;

	return chan << bf.offset;
}

static int s3cfb_setcolreg(unsigned int regno, unsigned int red, unsigned int green, unsigned int blue, unsigned int transp, struct fb_info *info)
{
	struct s3c_fb_info *fbi = (struct s3c_fb_info *)info;
	unsigned int val = 0;

	switch (fbi->fb.fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		if (regno < 16) {
			unsigned int *pal = fbi->fb.pseudo_palette;

			val = s3cfb_chan_to_field(red, fbi->fb.var.red);
			val |= s3cfb_chan_to_field(green, fbi->fb.var.green);
			val |= s3cfb_chan_to_field(blue, fbi->fb.var.blue);

			pal[regno] = val;
		}
		break;

	case FB_VISUAL_PSEUDOCOLOR:
		if (regno < 256) {
			val = ((red >> 0) & 0xf800);
			val |= ((green >> 5) & 0x07e0);
			val |= ((blue >> 11) & 0x001f);

			s3cfb_update_palette(fbi, regno, val);
		}
		break;

	default:
		return 1;	/* unknown type */
	}

	return 0;
}

static int s3cfb_open(struct fb_info *info, int user)
{
	struct s3c_fb_info *fbi = (struct s3c_fb_info *)info;

	fbi->wincon |= S3C_WINCONx_ENWIN_F_ENABLE;

	writel(fbi->wincon, S3C_WINCON(fbi->win_id));
}

static int s3cfb_release(struct fb_info *info, int user)
{
	struct s3c_fb_info *fbi = (struct s3c_fb_info *)info;

	fbi->wincon &= ~S3C_WINCONx_ENWIN_F_ENABLE;

	writel(fbi->wincon, S3C_WINCON(fbi->win_id));
}

struct fb_ops s3cfb_ops = {
	.owner		= THIS_MODULE,
	.fb_open	= s3cfb_open,
	.fb_release	= s3cfb_release,
	.fb_check_var	= s3cfb_check_var,
	.fb_set_par	= s3cfb_set_par,
	.fb_blank	= s3cfb_blank,
	.fb_pan_display	= s3cfb_pan_display,
	.fb_setcolreg	= s3cfb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
#if defined(CONFIG_FRAMEBUFFER_CONSOLE)
	.fb_cursor	= soft_cursor,
#endif
	.fb_ioctl	= s3cfb_ioctl,
};

/*
 * Framebuffer init
 */

static void s3cfb_init_fbinfo(struct s3c_fb_info *finfo,
					const char *drv_name, int index)
{
	int i = 0;

	strcpy(finfo->fb.fix.id, drv_name);

	finfo->win_id = index;
	finfo->fb.fix.type = FB_TYPE_PACKED_PIXELS;
	finfo->fb.fix.type_aux = 0;
	finfo->fb.fix.xpanstep = 0;
	finfo->fb.fix.ypanstep = 1;
	finfo->fb.fix.ywrapstep = 0;
	finfo->fb.fix.accel = FB_ACCEL_NONE;

	finfo->fb.fbops = &s3cfb_ops;
	finfo->fb.flags	= FBINFO_FLAG_DEFAULT;

	finfo->fb.pseudo_palette = &finfo->pseudo_pal;

	finfo->fb.var.nonstd = 0;
	finfo->fb.var.activate = FB_ACTIVATE_NOW;
	finfo->fb.var.accel_flags = 0;
	finfo->fb.var.vmode = FB_VMODE_NONINTERLACED;

	finfo->fb.var.xoffset = s3c_fimd.xoffset;
	finfo->fb.var.yoffset = s3c_fimd.yoffset;

	finfo->fb.var.height = s3c_fimd.height;
	finfo->fb.var.width = s3c_fimd.width;

	finfo->fb.var.xres = s3c_fimd.xres;
	finfo->fb.var.yres = s3c_fimd.yres;

	finfo->fb.var.xres_virtual = s3c_fimd.xres_virtual;
	finfo->fb.var.yres_virtual = s3c_fimd.yres_virtual;

	finfo->fb.var.bits_per_pixel = s3c_fimd.bpp;
	finfo->fb.var.pixclock = s3c_fimd.pixclock;
	finfo->fb.var.hsync_len = s3c_fimd.hsync_len;
	finfo->fb.var.left_margin = s3c_fimd.left_margin;
	finfo->fb.var.right_margin = s3c_fimd.right_margin;
	finfo->fb.var.vsync_len = s3c_fimd.vsync_len;
	finfo->fb.var.upper_margin = s3c_fimd.upper_margin;
	finfo->fb.var.lower_margin = s3c_fimd.lower_margin;
	finfo->fb.var.sync = s3c_fimd.sync;
	finfo->fb.var.grayscale = s3c_fimd.cmap_grayscale;

	finfo->fb.fix.smem_len =
		finfo->fb.var.xres_virtual * finfo->fb.var.yres_virtual * 4;

	finfo->fb.fix.line_length =
		finfo->fb.var.xres_virtual * s3c_fimd.bytes_per_pixel;

	for (i = 0; i < 256; i++)
		finfo->palette_buffer[i] = S3C_FB_PALETTE_BUFF_CLEAR;
}

static void s3cfb_pre_init(void)
{
	u32 div;
	int i;

	/* initialize the fimd specific */
	s3c_fimd.vidintcon0 &= ~S3C_VIDINTCON0_FRAMESEL0_MASK;
	s3c_fimd.vidintcon0 |= S3C_VIDINTCON0_FRAMESEL0_VSYNC;
	s3c_fimd.vidintcon0 |= S3C_VIDINTCON0_INTFRMEN_ENABLE;

	writel(s3c_fimd.vidintcon0, S3C_VIDINTCON0);

	/* Early Clock Setting to Sync Bootloader */
	lcd_clock = s3c_fimd.vidclk;

// KSS_2009-09-03 : Change LCD Dot Clk
#ifndef S3C_FB_USE_CLK_DIRECTED
	div = clk_get_rate(lcd_clock) / s3c_fimd.pixclock;
	if (((clk_get_rate(lcd_clock) * 10) / s3c_fimd.pixclock) % 10 <= 4) {
		--div;

	s3c_fimd.vidcon0 |= S3C_VIDCON0_CLKVAL_F(div);
#else
	/* TODO: Remove this */
	u32 div = __raw_readl(S3C_CLK_DIV1);
	div = (div & ~0xF000) | (12<<12);
	__raw_writel(div, S3C_CLK_DIV1);

	s3c_fimd.vidcon0 &= ~S3C_VIDCON0_CLKVAL_F(0xFF);
#endif	// End of S3C_FB_USE_CLK_DIRECTED

	/* Enable the display controller */
	s3c_fimd.vidcon0 |= S3C_VIDCON0_ENVID_ENABLE;
	s3c_fimd.vidcon0 |= S3C_VIDCON0_ENVID_F_ENABLE;
	writel(s3c_fimd.vidcon0, S3C_VIDCON0);

	/* Disable all windows */
	for (i = 0; i < S3C_FB_MAX_NUM; ++i)
		writel(0, S3C_WINCON(i));
}

/*
 *  Platform driver
 */

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend s3cfb_early_suspend_struct = {
	.suspend	= s3cfb_early_suspend,
	.resume		= s3cfb_late_resume,
	.level		= EARLY_SUSPEND_LEVEL_DISABLE_FB,
};
#endif

static int __init s3cfb_probe(struct platform_device *pdev)
{
	int index = 0, ret, i;
	struct clk *clk, *vidclk;
	struct s3c_fb_info *sfb;
	struct s3cfb_platform_data *pdata = pdev->dev.platform_data;

	memset(s3c_fb_info, 0, S3C_FB_NUM * sizeof(struct s3c_fb_info));

	clk = clk_get(NULL, "lcd");
	if (!clk || IS_ERR(clk)) {
		ERR("Failed to get lcd clock source\n");
		return -ENOENT;
	}

	vidclk = clk_get(NULL, "lcd_sclk");
	if (!vidclk || IS_ERR(vidclk)) {
		ERR("Failed to get lcd video clock source\n");
		ret = -ENOENT;
		goto release_clock;
	}

	clk_enable(clk);
	clk_enable(vidclk);

	INFO("LCD clock " FMT_MHZ " MHz, video clock " FMT_MHZ " MHz\n",
		PRINT_MHZ(clk_get_rate(clk)), PRINT_MHZ(clk_get_rate(vidclk)));

	s3c_fimd.clk = clk;
	s3c_fimd.vidclk = vidclk;

	msleep(5);

	/* Setup platform specific registers */
	s3cfb_set_fimd_info(pdata);

	s3cfb_pre_init();

	s3c_fimd.vsync_count = 0;
	init_waitqueue_head(&s3c_fimd.vsync_wait_queue);

	ret = request_irq(IRQ_LCD_VSYNC, s3cfb_irq, 0, DRIVER_NAME, pdev);
	if (ret != 0) {
		ERR("Failed to install irq (%d)\n", ret);
		goto release_vidclk;
	}

	sfb = &s3c_fb_info[0];
	for (index = 0; index < S3C_FB_NUM; ++index, ++sfb) {
		int cmap_size = 256;

		sfb->fb.par = sfb;
		sfb->fb.dev = &pdev->dev;
		sfb->dev = &pdev->dev;

		s3cfb_init_fbinfo(sfb, DRIVER_NAME, index);

		/* Initialize video memory */
		ret = s3cfb_map_video_memory(sfb);
		if (ret) {
			ERR("Failed to allocate video memory for fb %d (%d).\n",
								index, ret);
			--index;
			ret = -ENOMEM;
			goto free_video_memory;
		}

		if (index >= 2)
			cmap_size = 16;

		ret = fb_alloc_cmap(&sfb->fb.cmap, cmap_size, 0);
		if (ret < 0) {
			ERR("Failed to allocate color map for FB %d.\n", index);
			goto free_video_memory;
		}

		s3cfb_setup_window(sfb);

		ret = register_framebuffer(&sfb->fb);
		if (ret < 0) {
			ERR("Failed to register framebuffer device %d (%d)\n",
								index, ret);
			--index;
			goto free_cmap;
		}

		INFO("Registered %s frame buffer device (%d)\n",
						sfb->fb.fix.id, sfb->fb.node);
	}

	s3cfb_reset_unused_windows();

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&s3cfb_early_suspend_struct);
#endif	/* CONFIG_HAS_EARLYSUSPEND */

	return 0;

free_cmap:
	for(i = 0; i <= index; ++i)
		fb_dealloc_cmap(&s3c_fb_info[i].fb.cmap);
	++index;

free_video_memory:
	for(i = 0; i <= index; ++i)
		s3cfb_unmap_video_memory(&s3c_fb_info[i]);

	free_irq(IRQ_LCD_VSYNC, pdev);

release_vidclk:
#ifndef FB_S3C64XX_KEEP_POWER_ON_SHUTDOWN
	clk_disable(vidclk);
	clk_put(vidclk);
#endif

release_clock:
#ifndef FB_S3C64XX_KEEP_POWER_ON_SHUTDOWN
	clk_disable(clk);
	clk_put(clk);
#endif

	return ret;
}

/*
 *  Remove
 */
static int s3cfb_remove(struct platform_device *pdev)
{
	int index;

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&s3cfb_early_suspend_struct);
#endif	/* CONFIG_HAS_EARLYSUSPEND */

#ifndef FB_S3C64XX_KEEP_POWER_ON_SHUTDOWN
	s3cfb_stop_lcd();
	msleep(1);

	clk_disable(s3c_fimd.clk);
	clk_disable(s3c_fimd.vidclk);
	clk_put(s3c_fimd.clk);
	clk_put(s3c_fimd.vidclk);
#endif

	free_irq(IRQ_LCD_VSYNC, pdev);

	for (index = 0; index < S3C_FB_NUM; ++index) {
		unregister_framebuffer(&s3c_fb_info[index].fb);
		s3cfb_unmap_video_memory(&s3c_fb_info[index]);
	}

	return 0;
}

static struct platform_driver s3cfb_driver = {
	.probe		= s3cfb_probe,
	.remove		= s3cfb_remove,
	.suspend	= s3cfb_suspend,
	.resume		= s3cfb_resume,
	.shutdown	= s3cfb_shutdown,
        .driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

/*
 * Module
 */

int __devinit s3cfb_init(void)
{
	return platform_driver_register(&s3cfb_driver);
}

void __exit s3cfb_exit(void)
{
	platform_driver_unregister(&s3cfb_driver);
}

module_init(s3cfb_init);
module_exit(s3cfb_exit);

MODULE_AUTHOR("Tomasz Figa <tomasz.figa at gmail.com>");
MODULE_DESCRIPTION("S3C64xx Framebuffer Driver");
MODULE_LICENSE("GPL");
