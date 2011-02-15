/*
 * drivers/video/samsung/s3cfb.c
 *
 * $Id: s3cfb.c,v 1.1 2008/11/17 11:12:08 jsgood Exp $
 *
 * Copyright (C) 2008 Jinsung Yang <jsgood.yang@samsung.com>
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

#include "s3cfb.h"

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

struct s3c_fb_info s3c_fb_info[S3C_FB_NUM];

/*
 * Early suspend
 */

static struct early_suspend *early_suspend_ptr;

struct early_suspend *get_earlysuspend_ptr(void)
{
	return early_suspend_ptr;
}

/*
 * LCD power
 */

void s3cfb_set_lcd_power(int to)
{
	s3c_fimd.lcd_power = to;

	if (s3c_fimd.set_lcd_power)
		(s3c_fimd.set_lcd_power)(to);
}
EXPORT_SYMBOL(s3cfb_set_lcd_power);

void s3cfb_set_backlight_power(int to)
{
	s3c_fimd.backlight_power = to;

	if (s3c_fimd.set_backlight_power)
		(s3c_fimd.set_backlight_power)(to);
}
EXPORT_SYMBOL(s3cfb_set_backlight_power);

void s3cfb_set_backlight_level(int to)
{
	s3c_fimd.backlight_level = to;

	if (s3c_fimd.set_brightness)
		(s3c_fimd.set_brightness)(to);
}
EXPORT_SYMBOL(s3cfb_set_backlight_level);

/*
 * Memory mapping
 */

static int s3cfb_map_video_memory(struct s3c_fb_info *fbi)
{
	DPRINTK("map_video_memory(fbi=%p)\n", fbi);

	fbi->map_size_f1 = PAGE_ALIGN(fbi->fb.fix.smem_len);
	fbi->map_cpu_f1 = dma_alloc_writecombine(fbi->dev, fbi->map_size_f1,
						&fbi->map_dma_f1, GFP_KERNEL);

	if (!fbi->map_cpu_f1)
		return -ENOMEM;

	INFO("Allocated %d bytes of FB %d at %08x@%p.\n", fbi->map_size_f1,
				fbi->win_id, fbi->map_dma_f1, fbi->map_cpu_f1);

	/* prevent initial garbage on screen */
	memset(fbi->map_cpu_f1, 0x00, fbi->map_size_f1);

	fbi->fb.fix.smem_start = fbi->map_dma_f1;
	fbi->fb.screen_base = fbi->map_cpu_f1;

#if defined(CONFIG_FB_S3C64XX_DOUBLE_BUFFERING)
	if (fbi->win_id >= 2)
		return 0;

	fbi->map_size_f2 = (fbi->fb.fix.smem_len / 2);
	fbi->map_cpu_f2 = fbi->map_cpu_f1 + fbi->map_size_f2;
	fbi->map_dma_f2 = fbi->map_dma_f1 + fbi->map_size_f2;

	INFO("Buffer 2 of FB %d at %08x@%p.\n", fbi->win_id,
					fbi->map_dma_f2, fbi->map_cpu_f2);
#endif

	return 0;
}

static void s3cfb_unmap_video_memory(struct s3c_fb_info *fbi)
{
	dma_free_writecombine(fbi->dev, fbi->map_size_f1,
				fbi->map_cpu_f1, fbi->map_dma_f1);
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

	DPRINTK("s3c_fb_pan_display(var=%p, info=%p)\n", var, info);

	if (var->xoffset != 0)
	{
		printk("%s::var->xoffset(%d)\n", __func__, var->xoffset);
		return -EINVAL;
	}

	if (var->yoffset + info->var.yres > info->var.yres_virtual)
	{
		printk("%s::var->yoffset(%d) + info->var.yres(%d) > info->var.yres_virtual(%d)\n", __func__,
		        var->yoffset, info->var.yres, info->var.yres_virtual);
		return -EINVAL;
	}

	fbi->fb.var.xoffset = var->xoffset;
	fbi->fb.var.yoffset = var->yoffset;

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
	DPRINTK("blank(mode=%d, info=%p)\n", blank_mode, info);

	switch (blank_mode) {
	case VESA_NO_BLANKING:	/* lcd on, backlight on */
		s3cfb_set_lcd_power(1);
		s3cfb_set_backlight_power(1);
		break;

	case VESA_VSYNC_SUSPEND: /* lcd on, backlight off */
	case VESA_HSYNC_SUSPEND:
		s3cfb_set_lcd_power(1);
		s3cfb_set_backlight_power(0);
		break;

	case VESA_POWERDOWN: /* lcd and backlight off */
		s3cfb_set_lcd_power(0);
		s3cfb_set_backlight_power(0);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * Virtual screen extension
 */

#if defined(CONFIG_FB_S3C64XX_VIRTUAL_SCREEN)
int s3cfb_set_vs_info(s3c_vs_info_t vs_info)
{
	/* check invalid value */
	if (vs_info.width != s3c_fimd.xres || vs_info.height != s3c_fimd.yres)
		return 1;

	if (!(vs_info.bpp == 8 || vs_info.bpp == 16 || vs_info.bpp == 32))
		return 1;

	if (vs_info.offset < 0)
		return 1;

	if (vs_info.v_width != s3c_fimd.xres_virtual || vs_info.v_height != s3c_fimd.yres_virtual)
		return 1;

	/* save virtual screen information */
	s3c_fimd.vs_info = vs_info;

	if (s3c_fimd.vs_info.offset < 1)
		s3c_fimd.vs_info.offset = 1;

	if (s3c_fimd.vs_info.offset > S3C_FB_MAX_DISPLAY_OFFSET)
		s3c_fimd.vs_info.offset = S3C_FB_MAX_DISPLAY_OFFSET;

	s3c_fimd.vs_offset = s3c_fimd.vs_info.offset;

	return 0;
}
#endif

/*
 * Helper functions
 */

int s3cfb_onoff_win(struct s3c_fb_info *fbi, int onoff)
{
	int win_num =  fbi->win_id;

	if (onoff)
		writel(readl(S3C_WINCON0 + (0x04 * win_num)) | S3C_WINCONx_ENWIN_F_ENABLE, S3C_WINCON0 + (0x04 * win_num));
	else
		writel(readl(S3C_WINCON0 + (0x04 * win_num)) &~ (S3C_WINCONx_ENWIN_F_ENABLE), S3C_WINCON0 + (0x04 * win_num));

	return 0;
}

int s3cfb_onoff_color_key_alpha(struct s3c_fb_info *fbi, int onoff)
{
	int win_num =  fbi->win_id - 1;

	if (onoff)
		writel(readl(S3C_W1KEYCON0 + (0x08 * win_num)) | S3C_WxKEYCON0_KEYBLEN_ENABLE, S3C_W1KEYCON0 + (0x08 * win_num));
	else
		writel(readl(S3C_W1KEYCON0 + (0x08 * win_num)) &~ (S3C_WxKEYCON0_KEYBLEN_ENABLE), S3C_W1KEYCON0 + (0x08 * win_num));

	return 0;
}

int s3cfb_onoff_color_key(struct s3c_fb_info *fbi, int onoff)
{
	int win_num =  fbi->win_id - 1;

	if (onoff)
		writel(readl(S3C_W1KEYCON0 + (0x08 * win_num)) | S3C_WxKEYCON0_KEYEN_F_ENABLE, S3C_W1KEYCON0 + (0x08 * win_num));
	else
		writel(readl(S3C_W1KEYCON0 + (0x08 * win_num)) &~ (S3C_WxKEYCON0_KEYEN_F_ENABLE), S3C_W1KEYCON0 + (0x08 * win_num));

	return 0;
}

int s3cfb_set_color_key_registers(struct s3c_fb_info *fbi, s3c_color_key_info_t colkey_info)
{
	unsigned int compkey = 0;
	int win_num =  fbi->win_id;

	if (win_num == 0) {
		printk("WIN0 do not support color key\n");
		return -1;
	}

	win_num--;

	if (fbi->fb.var.bits_per_pixel == 16) {
		/* RGB 5-6-5 mode */
		compkey  = (((colkey_info.compkey_red & 0x1f) << 19) | 0x70000);
		compkey |= (((colkey_info.compkey_green & 0x3f) << 10) | 0x300);
		compkey |= (((colkey_info.compkey_blue  & 0x1f)  << 3 )| 0x7);
	} else if (fbi->fb.var.bits_per_pixel == 32) {
		/* currently RGBX 8-8-8-0 mode  */
		compkey  = ((colkey_info.compkey_red & 0xff) << 16);
		compkey |= ((colkey_info.compkey_green & 0xff) << 8);
		compkey |= ((colkey_info.compkey_blue & 0xff) << 0);
	} else
		printk("Invalid BPP has been given!\n");

	if (colkey_info.direction == S3C_FB_COLOR_KEY_DIR_BG)
		writel(S3C_WxKEYCON0_COMPKEY(compkey) | S3C_WxKEYCON0_DIRCON_MATCH_FG_IMAGE, S3C_W1KEYCON0 + (0x08 * win_num));

	else if (colkey_info.direction == S3C_FB_COLOR_KEY_DIR_FG)
		writel(S3C_WxKEYCON0_COMPKEY(compkey) | S3C_WxKEYCON0_DIRCON_MATCH_BG_IMAGE, S3C_W1KEYCON0 + (0x08 * win_num));

	else
		printk("Color key direction is not correct :: %d!\n", colkey_info.direction);

	return 0;
}

int s3cfb_set_color_value(struct s3c_fb_info *fbi, s3c_color_val_info_t colval_info)
{
	unsigned int colval = 0;

	int win_num =  fbi->win_id;

	if (win_num == 0) {
		printk("WIN0 do not support color key value\n");
		return -1;
	}

	win_num--;

	if (fbi->fb.var.bits_per_pixel == 16) {
		/* RGB 5-6-5 mode */
		colval  = (((colval_info.colval_red   & 0x1f) << 19) | 0x70000);
		colval |= (((colval_info.colval_green & 0x3f) << 10) | 0x300);
		colval |= (((colval_info.colval_blue  & 0x1f)  << 3 )| 0x7);
	} else if (fbi->fb.var.bits_per_pixel == 32) {
		/* currently RGB 8-8-8 mode  */
		colval  = ((colval_info.colval_red  & 0xff) << 16);
		colval |= ((colval_info.colval_green & 0xff) << 8);
		colval |= ((colval_info.colval_blue  & 0xff) << 0);
	} else
		printk("Invalid BPP has been given!\n");

	writel(S3C_WxKEYCON1_COLVAL(colval), S3C_W1KEYCON1 + (0x08 * win_num));

	return 0;
}

static int s3cfb_set_bpp(struct s3c_fb_info *fbi, int bpp)
{
	struct fb_var_screeninfo *var= &fbi->fb.var;
	int win_num =  fbi->win_id;
	unsigned int val;

	val = readl(S3C_WINCON0 + (0x04 * win_num));
	val &= ~(S3C_WINCONx_BPPMODE_F_MASK | S3C_WINCONx_BLD_PIX_MASK);
	val |= S3C_WINCONx_ALPHA_SEL_1;

	switch (bpp) {
	case 1:
	case 2:
	case 4:
	case 8:
		s3c_fimd.bytes_per_pixel = 1;
		break;

	case 16:
		writel(val | S3C_WINCONx_BPPMODE_F_16BPP_565 | S3C_WINCONx_BLD_PIX_PLANE, S3C_WINCON0 + (0x04 * win_num));
		var->bits_per_pixel = bpp;
		s3c_fimd.bytes_per_pixel = 2;
		break;

	case 24:
		bpp = 32;
		/* drop through */
	case 32:
		writel(val | S3C_WINCONx_BPPMODE_F_24BPP_888 | S3C_WINCONx_BLD_PIX_PLANE, S3C_WINCON0 + (0x04 * win_num));
		var->bits_per_pixel = bpp;
		s3c_fimd.bytes_per_pixel = 4;
		break;
	}

	return 0;
}

void s3cfb_stop_lcd(void)
{
	unsigned long flags;
	unsigned long tmp;

	local_irq_save(flags);

	tmp = readl(S3C_VIDCON0);
	if (tmp & S3C_VIDCON0_ENVID_ENABLE) {
		tmp &= ~S3C_VIDCON0_ENVID_F_ENABLE;
		writel(tmp, S3C_VIDCON0);
	}

	local_irq_restore(flags);
}

EXPORT_SYMBOL(s3cfb_stop_lcd);

void s3cfb_start_lcd(void)
{
	unsigned long flags;
	unsigned long tmp;

	local_irq_save(flags);

	tmp = readl(S3C_VIDCON0);
	if ((tmp & S3C_VIDCON0_ENVID_ENABLE) == 0) {
		tmp |= S3C_VIDCON0_ENVID_ENABLE | S3C_VIDCON0_ENVID_F_ENABLE;
		writel(tmp, S3C_VIDCON0);
	}

	local_irq_restore(flags);
}

EXPORT_SYMBOL(s3cfb_start_lcd);

void s3cfb_set_clock(unsigned int clkval)
{
	unsigned int tmp;

	tmp = readl(S3C_VIDCON0);

	tmp &= ~(0x1 << 4);
	tmp &= ~(0xff << 6);

	writel(tmp | (clkval << 6) | (1 << 4), S3C_VIDCON0);
}

EXPORT_SYMBOL(s3cfb_set_clock);

int s3cfb_init_win(struct s3c_fb_info *fbi, int bpp, int left_x, int top_y, int width, int height, int onoff)
{
	s3cfb_onoff_win(fbi, OFF);
	s3cfb_set_bpp(fbi, bpp);
	s3cfb_set_win_position(fbi, left_x, top_y, width, height);
	s3cfb_set_win_size(fbi, width, height);
	s3cfb_set_fb_size(fbi);
	s3cfb_onoff_win(fbi, onoff);

	return 0;
}

int s3cfb_wait_for_vsync(void)
{
	int cnt;

	cnt = s3c_fimd.vsync_info.count;
	wait_event_interruptible_timeout(s3c_fimd.vsync_info.wait_queue, cnt != s3c_fimd.vsync_info.count, HZ / 10);

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
//			if (info->var.bits_per_pixel == 16) {
				val = ((red >> 0) & 0xf800);
				val |= ((green >> 5) & 0x07e0);
				val |= ((blue >> 11) & 0x001f);
//			} else if (info->var.bits_per_pixel == 24) {
//				val = ((red << 8) & 0xff0000);
//				val |= ((green >> 0) & 0xff00);
//				val |= ((blue >> 8) & 0xff);
//			}

			DPRINTK("index = %d, val = 0x%08x\n", regno, val);
			s3cfb_update_palette(fbi, regno, val);
		}

		break;

	default:
		return 1;	/* unknown type */
	}

	return 0;
}

/*
 * Sysfs interface for power and backlight
 */

/* sysfs export of baclight control */
static int s3cfb_sysfs_show_lcd_power(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", s3c_fimd.lcd_power);
}

static int s3cfb_sysfs_store_lcd_power(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	if (len < 1)
		return -EINVAL;

	if (strnicmp(buf, "on", 2) == 0 || strnicmp(buf, "1", 1) == 0)
		{
		s3cfb_set_lcd_power(1);
		s3cfb_set_backlight_level(100);
		s3cfb_set_backlight_power(1);
		}
	else if (strnicmp(buf, "off", 3) == 0 || strnicmp(buf, "0", 1) == 0)
		s3cfb_set_lcd_power(0);
	else
		return -EINVAL;

	return len;
}

static int s3cfb_sysfs_show_backlight_power(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", s3c_fimd.backlight_power);
}

static int s3cfb_sysfs_store_backlight_power(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	if (len < 1)
		return -EINVAL;

	if (strnicmp(buf, "on", 2) == 0 || strnicmp(buf, "1", 1) == 0)
		s3cfb_set_backlight_power(1);
	else if (strnicmp(buf, "off", 3) == 0 || strnicmp(buf, "0", 1) == 0)
		s3cfb_set_backlight_power(0);
	else
		return -EINVAL;

	return len;
}

static int s3cfb_sysfs_show_backlight_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", s3c_fimd.backlight_level);
}

static int s3cfb_sysfs_store_backlight_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	unsigned long value = simple_strtoul(buf, NULL, 10);

	if (value < s3c_fimd.backlight_min || value > s3c_fimd.backlight_max)
		return -ERANGE;

	s3cfb_set_backlight_level(value);

	return len;
}

static DEVICE_ATTR(lcd_power, 0777,
			s3cfb_sysfs_show_lcd_power,
			s3cfb_sysfs_store_lcd_power);

static DEVICE_ATTR(backlight_power, 0644,
			s3cfb_sysfs_show_backlight_power,
			s3cfb_sysfs_store_backlight_power);

static DEVICE_ATTR(backlight_level, 0644,
			s3cfb_sysfs_show_backlight_level,
			s3cfb_sysfs_store_backlight_level);

struct fb_ops s3cfb_ops = {
	.owner		= THIS_MODULE,
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

	if (index < 2)
	{
		finfo->fb.var.height = s3c_fimd.height;
		finfo->fb.var.width = s3c_fimd.width;

		finfo->fb.var.xres = s3c_fimd.xres;
		finfo->fb.var.yres = s3c_fimd.yres;

		finfo->fb.var.xres_virtual = s3c_fimd.xres_virtual;
		finfo->fb.var.yres_virtual = s3c_fimd.yres_virtual;
	}
	else
	{
		finfo->fb.var.height = s3c_fimd.osd_height;
		finfo->fb.var.width = s3c_fimd.osd_width;

		finfo->fb.var.xres = s3c_fimd.osd_xres;
		finfo->fb.var.yres = s3c_fimd.osd_yres;

		finfo->fb.var.xres_virtual = s3c_fimd.osd_xres_virtual;
		finfo->fb.var.yres_virtual = s3c_fimd.osd_yres_virtual;
	}

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

	s3cfb_pre_init();
	s3cfb_set_backlight_power(1);
	s3cfb_set_lcd_power(1);
	s3cfb_set_backlight_level(S3C_FB_DEFAULT_BACKLIGHT_LEVEL);

	s3c_fimd.vsync_info.count = 0;
	init_waitqueue_head(&s3c_fimd.vsync_info.wait_queue);

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

		s3cfb_init_registers(sfb);
		s3cfb_check_var(&sfb->fb.var, &sfb->fb);

		if (index >= 2)
			cmap_size = 16;

		ret = fb_alloc_cmap(&sfb->fb.cmap, cmap_size, 0);
		if (ret < 0) {
			ERR("Failed to allocate color map for FB %d.\n", index);
			goto free_video_memory;
		}

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

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&s3cfb_early_suspend_struct);
#endif	/* CONFIG_HAS_EARLYSUSPEND */

	/* create device files */
	ret = device_create_file(&(pdev->dev), &dev_attr_backlight_power);
	if (ret < 0)
		WARNING("Failed to add entries\n");

	ret = device_create_file(&(pdev->dev), &dev_attr_backlight_level);
	if (ret < 0)
		WARNING("Failed to add entries\n");

	ret = device_create_file(&(pdev->dev), &dev_attr_lcd_power);
	if (ret < 0)
		WARNING("Failed to add entries\n");

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
		framebuffer_release(&s3c_fb_info[index].fb);
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
static void __exit s3cfb_cleanup(void)
{
	platform_driver_unregister(&s3cfb_driver);
}

module_init(s3cfb_init);
module_exit(s3cfb_cleanup);

MODULE_AUTHOR("Tomasz Figa <tomasz.figa at gmail.com>");
MODULE_DESCRIPTION("S3C64xx Framebuffer Driver");
MODULE_LICENSE("GPL");
