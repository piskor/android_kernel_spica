/* linux/arch/arm/mach-s3c64xx/gpio-config.c
 *
 * Copyright 2010 Tomasz Figa <tomasz.figa at gmail.com>
 *
 * S3C64XX series GPIO configuration core extensions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/io.h>

#include <plat/gpio-core.h>
#include <mach/gpio-cfg.h>

#include <mach/map.h>
#include <mach/regs-gpio.h>

#define IN_RANGE(value,low,high) ((value >= low) && (value < high))

void s3c_init_gpio(struct s3c_gpio_config *table, unsigned int count)
{
	u32 i;
	struct s3c_gpio_config *gpio = table;

	for (i = 0; i < count; i++) {
		s3c_gpio_cfgpin(gpio->num, S3C_GPIO_SFN(gpio->cfg));
		s3c_gpio_setpull(gpio->num, gpio->pull);

		if (gpio->lvl != GPIO_LEVEL_NONE)
			gpio_set_value(gpio->num, gpio->lvl);

		if (IN_RANGE(gpio->num, S3C64XX_GPIO_OFF_PART_BASE, 
						S3C64XX_GPIO_MEM_PART_BASE))
			continue;

		s3c_gpio_slp_cfgpin(gpio->num, gpio->slp_cfg);
		s3c_gpio_slp_setpull_updown(gpio->num, gpio->slp_pull);
	}
}

int s3c_gpio_slp_cfgpin(unsigned int pin, unsigned int config)
{
	struct s3c_gpio_chip *chip = s3c_gpiolib_getchip(pin);
	void __iomem *reg;
	unsigned long flags;
	int offset;
	u32 con;
	int shift;

	if (!chip)
		return -EINVAL;
	if((chip->base == (S3C64XX_GPK_BASE + 0x4)) ||
		(chip->base == (S3C64XX_GPL_BASE + 0x4)) ||
		(chip->base == S3C64XX_GPM_BASE) ||
		(chip->base == S3C64XX_GPN_BASE))
	{
		return -EINVAL;
	}

	if(config > 3)
	{
			return -EINVAL;
	}

	reg = chip->base + 0x0C;

	offset = pin - chip->chip.base;
	shift = offset * 2;

	local_irq_save(flags);

	con = __raw_readl(reg);
	con &= ~(3 << shift);
	con |= config << shift;
		__raw_writel(con, reg);

	local_irq_restore(flags);

	return 0;
}
EXPORT_SYMBOL(s3c_gpio_slp_cfgpin);

s3c_gpio_pull_t s3c_gpio_get_slp_cfgpin(unsigned int pin)
{
	struct s3c_gpio_chip *chip = s3c_gpiolib_getchip(pin);
	void __iomem *reg;
	unsigned long flags;
	int offset;
	u32 con;
	int shift;

	if (!chip)
		return -EINVAL;
	if((chip->base == (S3C64XX_GPK_BASE + 0x4)) ||
		(chip->base == (S3C64XX_GPL_BASE + 0x4)) ||
		(chip->base == S3C64XX_GPM_BASE) ||
		(chip->base == S3C64XX_GPN_BASE))
	{
		return -EINVAL;
	}

	reg = chip->base + 0x0C;

	offset = pin - chip->chip.base;
	shift = offset * 2;

	local_irq_save(flags);

	con = __raw_readl(reg);
	con >>= shift;
	con &= 0x3;

	local_irq_restore(flags);

	return (__force s3c_gpio_pull_t)con;
}
EXPORT_SYMBOL(s3c_gpio_get_slp_cfgpin);

int s3c_gpio_slp_setpull_updown(unsigned int pin, unsigned int config)
{
	struct s3c_gpio_chip *chip = s3c_gpiolib_getchip(pin);
	void __iomem *reg;
	unsigned long flags;
	int offset;
	u32 con;
	int shift;

	if (!chip)
		return -EINVAL;
	if((chip->base == (S3C64XX_GPK_BASE + 0x4)) ||
		(chip->base == (S3C64XX_GPL_BASE + 0x4)) ||
		(chip->base == S3C64XX_GPM_BASE) ||
		(chip->base == S3C64XX_GPN_BASE))
	{
		return -EINVAL;
	}
	if(config > 3)
	{
		return -EINVAL;
	}
	reg = chip->base + 0x10;

	offset = pin - chip->chip.base;
	shift = offset * 2;

	local_irq_save(flags);

	con = __raw_readl(reg);
	con &= ~(3 << shift);
	con |= config << shift;
	__raw_writel(con, reg);

	con = __raw_readl(reg);

	local_irq_restore(flags);

	return 0;
}
EXPORT_SYMBOL(s3c_gpio_slp_setpull_updown);

s3c_gpio_pull_t s3c_gpio_slp_getpull_updown(unsigned int pin)
{
	struct s3c_gpio_chip *chip = s3c_gpiolib_getchip(pin);
	void __iomem *reg;
	unsigned long flags;
	int offset;
	u32 con;
	int shift;

	if (!chip)
		return -EINVAL;
	if((chip->base == (S3C64XX_GPK_BASE + 0x4)) ||
		(chip->base == (S3C64XX_GPL_BASE + 0x4)) ||
		(chip->base == S3C64XX_GPM_BASE) ||
		(chip->base == S3C64XX_GPN_BASE))
	{
		return -EINVAL;
	}

	reg = chip->base + 0x10;

	offset = pin - chip->chip.base;
	shift = offset * 2;

	local_irq_save(flags);

	con = __raw_readl(reg);
	con >>= shift;
	con &= 0x3;

	local_irq_restore(flags);

	return (__force s3c_gpio_pull_t)con;
}
EXPORT_SYMBOL(s3c_gpio_slp_getpull_updown);
