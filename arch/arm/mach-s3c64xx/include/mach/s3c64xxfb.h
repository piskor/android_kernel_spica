/* include/video/s3cfb.h
 *
 * Copyright 2011 Tomasz Figa <tomasz.figa at gmail.com>
 *
 * S3C64XX framebuffer platform data
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _S3C64XXFB_H_
#define _S3C64XXFB_H_

struct s3c64xxfb_platform_data {
	/* Physical size */
	u32 width;
	u32 height;

	/* Screen resolution */
	u32 xres;
	u32 yres;

	/* Synchronization */
	u32 hfp;
	u32 hsw;
	u32 hbp;
	u32 vfp;
	u32 vsw;
	u32 vbp;

	/* Dither position 0 or 0xRGB where R/G/B is one of {5,6,8} */
	u32 dither;

	/* Vidcon1 */
	u32 vidcon1;

	/* Refresh rate in Hz */
	u32 refresh_rate;

	/* Boot logo */
	u32 logo_base;
	u32 logo_size;
};

#endif /* _S3CFB_H_ */
