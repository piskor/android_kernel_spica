/*
 * drivers/video/samsung/s3cfb.h
 *
 * $Id: s3cfb.h,v 1.1 2008/11/17 11:12:08 jsgood Exp $
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

#ifndef _S3CFB_H_
#define _S3CFB_H_

#include <linux/interrupt.h>
#include <linux/earlysuspend.h>

#if  defined(CONFIG_S3C6410_PWM)
extern int s3c6410_timer_setup (int channel, int usec, unsigned long g_tcnt, unsigned long g_tcmp);
#endif

/*
 *  Debug macros
 */
#define DEBUG 0

#if DEBUG
#define DPRINTK(fmt, args...)	printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

/*
 *  Definitions
 */
#ifndef MHZ
#define MHZ (1000 * 1000)
#endif

#define S3C_FB_OUTPUT_RGB	0
#define S3C_FB_OUTPUT_TV	1
#define S3C_FB_OUTPUT_I80_LDI0	2
#define S3C_FB_OUTPUT_I80_LDI1	3

#define S3C_FB_PALETTE_BUFF_CLEAR	(0x80000000)	/* entry is clear/invalid */
#define S3C_FB_MAX_ALPHA_LEVEL		(0xF)

#if defined(CONFIG_MACH_SPICA)
	#define S3C_FB_USE_CLK_DIRECTED // KSS_2009-09-03 : Change LCD Dot Clk
#endif

/*
 *  macros
 */
#define FMT_MHZ				"%ld.%03ld"
#define PRINT_MHZ(m) 			((m) / MHZ), ((m / 1000) % 1000)

#define S3C_FB_MAX_NUM			5
#define FB_MAX_NUM(x, y)		((x) > (y) ? (y) : (x))
#define S3C_FB_NUM			FB_MAX_NUM(S3C_FB_MAX_NUM, CONFIG_FB_S3C64XX_NUM)

/*
 *  structures
 */

typedef struct {
	struct fb_bitfield red;
	struct fb_bitfield green;
	struct fb_bitfield blue;
	struct fb_bitfield transp;
} s3c_fb_rgb_t;

const static s3c_fb_rgb_t s3c_fb_rgb_8 = {
	.red    = {.offset = 0,  .length = 8,},
	.green  = {.offset = 0,  .length = 8,},
	.blue   = {.offset = 0,  .length = 8,},
	.transp = {.offset = 0,  .length = 0,},
};

const static s3c_fb_rgb_t s3c_fb_rgb_16 = {
	.red    = {.offset = 11, .length = 5,},
	.green  = {.offset = 5,  .length = 6,},
	.blue   = {.offset = 0,  .length = 5,},
	.transp = {.offset = 0,  .length = 0,},
};

const static s3c_fb_rgb_t s3c_fb_rgb_24 = {
	.red    = {.offset = 16, .length = 8,},
	.green  = {.offset = 8,  .length = 8,},
	.blue   = {.offset = 0,  .length = 8,},
	.transp = {.offset = 0,  .length = 0,},
};

struct s3c_fb_info {
	struct fb_info		fb;
	unsigned int		win_id;
	struct device		*dev;

	/* raw memory addresses */
	dma_addr_t		map_dma;	/* physical */
	u_char *		map_cpu;	/* virtual */
	unsigned int		map_size;	/* size */

	unsigned int		palette_ready;

	/* keep these registers in case we need to re-write palette */
	unsigned int		palette_buffer[256];
	unsigned int		pseudo_pal[16];

	/* window spcific state */
	u32			wincon;
	u32			vidosda;
	u32			vidosdb;
	u32			vidosdc;
	u32			vidosdd;
	u32			bpp;
	u32			bytes_per_pixel;
	u32			fb_start_addr;
	u32			fb_end_addr;
};

struct s3c_fb_drvdata {
	/* Clocks */
	struct clk		*clk;
	struct clk		*vidclk;

	/* Screen size */
	int width;
	int height;

	/* Screen info */
	int xres;
	int yres;

	/* Virtual Screen info */
	int xres_virtual;
	int yres_virtual;
	int xoffset;
	int yoffset;

	unsigned long pixclock;

	int hsync_len;
	int left_margin;
	int right_margin;
	int vsync_len;
	int upper_margin;
	int lower_margin;
	int sync;

	int cmap_grayscale:1;
	int cmap_inverse:1;
	int cmap_static:1;
	int unused:29;

	int vs_offset;
	int palette_win;

	wait_queue_head_t vsync_wait_queue;
	unsigned long vsync_count;

	/* FIMD registers */
	unsigned long vidcon0;
	unsigned long vidcon1;
	unsigned long vidtcon0;
	unsigned long vidtcon1;
	unsigned long vidtcon2;
	unsigned long vidtcon3;

	unsigned long vidintcon;
	unsigned long vidintcon0;
	unsigned long vidintcon1;
	unsigned long w1keycon0;
	unsigned long w1keycon1;
	unsigned long w2keycon0;
	unsigned long w2keycon1;
	unsigned long w3keycon0;
	unsigned long w3keycon1;
	unsigned long w4keycon0;
	unsigned long w4keycon1;

	unsigned long win0map;
	unsigned long win1map;
	unsigned long win2map;
	unsigned long win3map;
	unsigned long win4map;

	unsigned long wpalcon;
	unsigned long dithmode;
	unsigned long intclr0;
	unsigned long intclr1;
	unsigned long intclr2;

	unsigned long win0pal;
	unsigned long win1pal;

	/* Boot logo */
	u32 logo_base;
	u32 logo_size;
};

#endif
