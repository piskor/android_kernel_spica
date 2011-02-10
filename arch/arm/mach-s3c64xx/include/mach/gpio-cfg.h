/* linux/arch/arm/mach-s3c64xx/include/mach/gpio-cfg.h
 *
 * Copyright 2011 Tomasz Figa <tomasz.figa at gmail.com>
 * S3C64XX Platform - extended GPIO pin configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __MACH_GPIO_CFG_H
#define __MACH_GPIO_CFG_H __FILE__

#include <plat/gpio-cfg.h>

/* Output low */
#define S3C_GPIO_SLP_OUT0	((__force s3c_gpio_pull_t)0x00)
/* Output high */
#define S3C_GPIO_SLP_OUT1	((__force s3c_gpio_pull_t)0x01)
/* Input */
#define S3C_GPIO_SLP_INPUT	((__force s3c_gpio_pull_t)0x02)
/* Previous state */
#define S3C_GPIO_SLP_PREV	((__force s3c_gpio_pull_t)0x03)

/**
 * s3c_gpio_slp_cfgpin() - Change the GPIO function of a pin in sleep mode.
 * @pin pin The pin number to configure.
 * @to to The configuration for the pin's function.
 *
 * Configure which function is actually connected to the external
 * pin in sleep mode, such as an gpio input, output or some form of special
 * function connected to an internal peripheral block.
 *
 * The @to parameter can be one of the generic S3C_GPIO_SLP_* values.
 *
 * Sleep mode configuration is supported only by GPIO pins from
 * ALIVE and MEM parts.
 */
extern int s3c_gpio_slp_cfgpin(unsigned int pin, unsigned int to);

/**
 * s3c_gpio_get_slp_cfgpin - Read the GPIO function of a pin in sleep mode.
 * @pin: The pin to read the configuration value for.
 *
 * Read the configuration state of the given @pin, returning a value that
 * could be passed back to s3c_gpio_slp_cfgpin().
 *
 * @sa s3c_gpio_slp_cfgpin
 */
extern s3c_gpio_pull_t s3c_gpio_get_slp_cfgpin(unsigned int pin);

/**
 * s3c_gpio_slp_setpull_updown() - set the state of a gpio pin pull resistor
 * in sleep mode
 * @pin: The pin number to configure the pull resistor.
 * @pull: The configuration for the pull resistor.
 *
 * This function sets the state of the pull-{up,down} resistor for the
 * specified pin. It will return 0 if successfull, or a negative error
 * code if the pin cannot support the requested pull setting.
 *
 * @pull is one of S3C_GPIO_PULL_NONE, S3C_GPIO_PULL_DOWN or S3C_GPIO_PULL_UP.
*/
extern int s3c_gpio_slp_setpull_updown(unsigned int pin, s3c_gpio_pull_t pull);


/**
 * s3c_gpio_slp_getpull_updown() - get the pull resistor state of a gpio pin
 * in sleep mode
 * @pin: The pin number to get the settings for
 *
 * Read the pull resistor value for the specified pin.
*/
extern s3c_gpio_pull_t s3c_gpio_slp_getpull_updown(unsigned int pin);

/* GPIO levels for lvl field of s3c64xx_gpio_config struct */
#define GPIO_LEVEL_LOW      	0
#define GPIO_LEVEL_HIGH     	1
#define GPIO_LEVEL_NONE     	2

/**
 * struct s3c_gpio_config GPIO configuration
 * @num: Pin number
 * @cfg: Pin function
 * @lvl: Pin level
 * @pull: Pin pull mode
 * @slp_cfg: Pin function in sleep mode
 * @slp_pull: Pin pull mode in sleep mode
 *
 * Structure used by machine specific GPIO configuration tables for quick
 * GPIO pin initialization using s3c_init_gpio.
 *
 * @sa s3c_gpio_cfgpin
 * @sa s3c_gpio_getcfg
 * @sa s3c_gpio_setpull
 * @sa s3c_gpio_getpull
 * @sa s3c_gpio_slp_cfgpin
 * @sa s3c_gpio_get_slp_cfgpin
 * @sa s3c_gpio_slp_setpull_updown
 * @sa s3c_gpio_slp_getpull_updown
 */
struct s3c_gpio_config {
	unsigned int num;

	unsigned int cfg;
	unsigned int lvl;
	unsigned int pull;

	unsigned int slp_cfg;
	unsigned int slp_pull;
};

/**
 * s3c_init_gpio() - setup GPIO pins using given configuration table.
 * @table: Configuration table
 * @count: Number of records in configuration table
 *
 * Initializes GPIO pins using passed configuration table.
*/
extern void s3c_init_gpio(struct s3c_gpio_config *table, unsigned int count);

#endif /* __MACH_GPIO_CFG_H */
