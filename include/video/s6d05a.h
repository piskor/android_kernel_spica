/* include/video/s6d05a.h
 *
 * Copyright 2011 Tomasz Figa <tomasz.figa at gmail.com>
 *
 * Platform data for S6D05A LCD controller.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#ifndef _S6D05A_H_
#define _S6D05A_H_

/* SPI command codes */
#define PWRCTL			0xF3
#define SLPIN			0x10
#define SLPOUT			0x11
#define DISCTL			0xF2
#define VCMCTL			0xF4
#define SRCCTL			0xF5
#define GATECTL			0xFD
#define MADCTL			0x36
#define COLMOD			0x3A
#define GAMCTL1			0xF7
#define GAMCTL2			0xF8
#define GAMCTL3			0xF9
#define GAMCTL4			0xFA
#define GAMCTL5			0xFB
#define GAMCTL6			0xFC
#define BCMODE			0xCB
#define WRCABC			0x55
#define DCON			0xEF
#define WRCTRLD			0x53
#define WRDISBV			0x51
#define WRCABCMB		0x5E
#define MIECTL1			0xCA
#define MIECTL2			0xCC
#define MIECTL3			0xCD

/* Command descriptor */
struct s6d05a_command {
	u8 command;
	u8 parameters;
	u8 parameter[15];
	s32 wait;
};

/* Platform data */
struct s6d05a_pdata {
	/* Required: Reset GPIO and power control function */
	unsigned reset_gpio;
	void (*set_power)(int);

	/* Optional: override of power on command sequence */
	struct s6d05a_command *power_on_seq;
	u32 power_on_seq_len;

	/* Optional: override of power off command sequence */
	struct s6d05a_command *power_off_seq;
	u32 power_off_seq_len;
};

#endif /* _S6D05A_H_ */
