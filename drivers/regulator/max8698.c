/*
 * max8698.c - Maxim MAX8698 voltage regulator driver
 *
 * Copyright (C) 2011 Tomasz Figa <tomasz.figa at gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/max8698.h>

struct max8698_data {
	struct device		*dev;
	struct max8698_dev	*iodev;
	int			num_regulators;
	struct regulator_dev	**rdev;
};

struct voltage_map_desc {
	int min;
	int max;
	int step;
};

/* Voltage maps */
static const struct voltage_map_desc ldo23_voltage_map_desc = {
	.min = 800,	.step = 50,	.max = 1300,
};
static const struct voltage_map_desc ldo45679_voltage_map_desc = {
	.min = 1600,	.step = 100,	.max = 3600,
};
static const struct voltage_map_desc ldo8_voltage_map_desc = {
	.min = 3000,	.step = 100,	.max = 3600,
};
static const struct voltage_map_desc buck12_voltage_map_desc = {
	.min = 750,	.step = 50,	.max = 1500,
};
static const struct voltage_map_desc buck3_voltage_map_desc = {
	.min = 1600,	.step = 100,	.max = 3600,
};

static const struct voltage_map_desc *ldo_voltage_map[] = {
	&ldo23_voltage_map_desc,	/* LDO2 */
	&ldo23_voltage_map_desc,	/* LDO3 */
	&ldo45679_voltage_map_desc,	/* LDO4 */
	&ldo45679_voltage_map_desc,	/* LDO5 */
	&ldo45679_voltage_map_desc,	/* LDO6 */
	&ldo45679_voltage_map_desc,	/* LDO7 */
	&ldo8_voltage_map_desc,		/* LDO8 */
	&ldo45679_voltage_map_desc,	/* LDO9 */
	&buck12_voltage_map_desc,	/* DVSARM1 */
	&buck12_voltage_map_desc,	/* DVSARM2 */
	&buck12_voltage_map_desc,	/* DVSARM3 */
	&buck12_voltage_map_desc,	/* DVSARM4 */
	&buck12_voltage_map_desc,	/* DVSINT1 */
	&buck12_voltage_map_desc,	/* DVSINT2 */
	&buck3_voltage_map_desc,	/* BUCK3 */
};

static inline int max8698_get_ldo(struct regulator_dev *rdev)
{
	return rdev_get_id(rdev);
}

static int max8698_list_voltage(struct regulator_dev *rdev,
				unsigned int selector)
{
	const struct voltage_map_desc *desc;
	int ldo = max8698_get_ldo(rdev);
	int val;

	if (ldo >= ARRAY_SIZE(ldo_voltage_map))
		return -EINVAL;

	desc = ldo_voltage_map[ldo];
	if (desc == NULL)
		return -EINVAL;

	val = desc->min + desc->step * selector;
	if (val > desc->max)
		return -EINVAL;

	return val * 1000;
}

enum {
	MAX8698_REG_ONOFF1,
	MAX8698_REG_ONOFF2,
	MAX8698_REG_ADISCHG_EN1,
	MAX8698_REG_ADISCHG_EN2,
	MAX8698_REG_DVSARM12,
	MAX8698_REG_DVSARM34,
	MAX8698_REG_DVSINT12,
	MAX8698_REG_BUCK3,
	MAX8698_REG_LDO23,
	MAX8698_REG_LDO4,
	MAX8698_REG_LDO5,
	MAX8698_REG_LDO6,
	MAX8698_REG_LDO7,
	MAX8698_REG_LDO8_BKCHR,
	MAX8698_REG_LD09,
	MAX8698_REG_LBCNFG
};

static int max8698_get_enable_register(struct regulator_dev *rdev,
					int *reg, int *shift)
{
	int ldo = max8698_get_ldo(rdev);

	switch (ldo) {
	case MAX8698_LDO2 ... MAX8698_LDO5:
		*reg = MAX8698_REG_ONOFF1;
		*shift = 4 - (ldo - MAX8698_LDO2);
		break;
	case MAX8698_LDO6 ... MAX8698_LDO9:
		*reg = MAX8698_REG_ONOFF2;
		*shift = 7 - (ldo - MAX8698_LDO6);
		break;
	case MAX8698_DVSARM1 ... MAX8698_DVSARM4:
		*reg = MAX8698_REG_ONOFF1;
		*shift = 7;
		break;
	case MAX8698_DVSINT1 ... MAX8698_DVSINT2:
		*reg = MAX8698_REG_ONOFF1;
		*shift = 6;
		break;
	case MAX8698_BUCK3:
		*reg = MAX8698_REG_ONOFF1;
		*shift = 5;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int max8698_ldo_is_enabled(struct regulator_dev *rdev)
{
	struct max8698_data *max8698 = rdev_get_drvdata(rdev);
	int ret, reg, shift = 8;
	u8 val;

	ret = max8698_get_enable_register(rdev, &reg, &shift);
	if (ret)
		return ret;

	ret = max8698_read_reg(max8698->iodev, reg, &val);
	if (ret)
		return ret;

	return val & (1 << shift);
}

static int max8698_ldo_enable(struct regulator_dev *rdev)
{
	struct max8698_data *max8698 = rdev_get_drvdata(rdev);
	int reg, shift = 8, ret;

	ret = max8698_get_enable_register(rdev, &reg, &shift);
	if (ret)
		return ret;

	return max8698_update_reg(max8698->iodev, reg, 1<<shift, 1<<shift);
}

static int max8698_ldo_disable(struct regulator_dev *rdev)
{
	struct max8698_data *max8698 = rdev_get_drvdata(rdev);
	int reg, shift = 8, ret;

	ret = max8698_get_enable_register(rdev, &reg, &shift);
	if (ret)
		return ret;

	return max8698_update_reg(max8698->iodev, reg, 0, 1<<shift);
}

static int max8698_get_voltage_register(struct regulator_dev *rdev,
				int *_reg, int *_shift, int *_mask)
{
	int ldo = max8698_get_ldo(rdev);
	int reg, shift = 0, mask = 0xff;

	switch (ldo) {
	case MAX8698_LDO2 ... MAX8698_LDO3:
		reg = MAX8698_REG_LDO23;
		mask = 0xf;
		if (ldo == MAX8698_LDO3)
			shift = 4;
	case MAX8698_LDO4 ... MAX8698_LDO7:
		reg = MAX8698_REG_LDO4 + (ldo - MAX8698_LDO4);
		break;
	case MAX8698_LDO8:
		reg = MAX8698_REG_LDO8_BKCHR;
		mask = 0xf;
		shift = 4;
		break;
	case MAX8698_LDO9:
		reg = MAX8698_REG_LDO10_LDO11;
		break;
	case MAX8698_DVSARM1:
	case MAX8698_DVSARM2:
		reg = MAX8698_REG_DVSARM12;
		mask = 0xf;
		if (ldo == MAX8698_DVSARM2)
			shift = 4;
		break;
	case MAX8698_DVSARM3:
	case MAX8698_DVSARM4:
		reg = MAX8698_REG_DVSARM34;
		mask = 0xf;
		if (ldo = MAX8698_DVSARM4)
			shift = 4;
		break;
	case MAX8698_DVSINT1:
	case MAX8698_DVSINT2:
		reg = MAX8698_REG_DVSINT12;
		mask = 0xf;
		if (ldo = MAX8698_DVSINT2)
			shift = 4;
	case MAX8698_BUCK3:
		reg = MAX8698_REG_BUCK3;
		break;
	default:
		return -EINVAL;
	}

	*_reg = reg;
	*_shift = shift;
	*_mask = mask;

	return 0;
}

static int max8698_get_voltage(struct regulator_dev *rdev)
{
	struct max8698_data *max8698 = rdev_get_drvdata(rdev);
	int reg, shift = 0, mask, ret;
	u8 val;

	ret = max8698_get_voltage_register(rdev, &reg, &shift, &mask);
	if (ret)
		return ret;

	ret = max8698_read_reg(max8698->iodev, reg, &val);
	if (ret)
		return ret;

	val >>= shift;
	val &= mask;

	return max8698_list_voltage(rdev, val);
}

static int max8698_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV)
{
	struct max8698_data *max8698 = rdev_get_drvdata(rdev);
	int min_vol = min_uV / 1000, max_vol = max_uV / 1000;
	int previous_vol = 0;
	const struct voltage_map_desc *desc;
	int ldo = max8698_get_ldo(rdev);
	int reg, shift = 0, mask, ret;
	int i = 0;
	u8 val;
	bool en_ramp = false;

	if (ldo >= ARRAY_SIZE(ldo_voltage_map))
		return -EINVAL;

	desc = ldo_voltage_map[ldo];
	if (desc == NULL)
		return -EINVAL;

	if (max_vol < desc->min || min_vol > desc->max)
		return -EINVAL;

	while (desc->min + desc->step*i < min_vol &&
	       desc->min + desc->step*i < desc->max)
		i++;

	if (desc->min + desc->step*i > max_vol)
		return -EINVAL;

	ret = max8698_get_voltage_register(rdev, &reg, &shift, &mask);
	if (ret)
		return ret;

	/* wait for RAMP_UP_DELAY if rdev is BUCK1/2 */
	if (ldo >= MAX8698_DVSARM1 && ldo <= MAX8698_DVSINT2) {
		int difference, rate;

		/* Read ramp rate */
		max8698_read_reg(max8698->iodev, MAX8698_REG_ADISCHG_EN2, &val);
		rate = (val & 0xf) + 1;

		previous_vol = max8698_get_voltage(rdev);

		ret = max8698_update_reg(max8698->iodev, reg, i<<shift, mask<<shift);

		difference = desc->min + desc->step*i - previous_vol/1000;
		if (difference < 0)
			difference = -difference;

		udelay(difference / rate);
	}

	return ret;
}

static struct regulator_ops max8698_regulator_ops = {
	.list_voltage		= max8698_list_voltage,
	.is_enabled		= max8698_ldo_is_enabled,
	.enable			= max8698_ldo_enable,
	.disable		= max8698_ldo_disable,
	.get_voltage		= max8698_get_voltage,
	.set_voltage		= max8698_set_voltage,
	.set_suspend_enable	= max8698_ldo_enable,
	.set_suspend_disable	= max8698_ldo_disable,
};

static struct regulator_desc regulators[] = {
	{
		.name		= "LDO2",
		.id		= MAX8698_LDO2,
		.ops		= &max8698_regulator_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "LDO3",
		.id		= MAX8698_LDO3,
		.ops		= &max8698_regulator_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "LDO4",
		.id		= MAX8698_LDO4,
		.ops		= &max8698_regulator_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "LDO5",
		.id		= MAX8698_LDO5,
		.ops		= &max8698_regulator_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "LDO6",
		.id		= MAX8698_LDO6,
		.ops		= &max8698_regulator_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "LDO7",
		.id		= MAX8698_LDO7,
		.ops		= &max8698_regulator_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "LDO8",
		.id		= MAX8698_LDO8,
		.ops		= &max8698_regulator_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "LDO9",
		.id		= MAX8698_LDO9,
		.ops		= &max8698_regulator_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "DVSARM1",
		.id		= MAX8698_DVSARM1,
		.ops		= &max8698_regulator_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "DVSARM2",
		.id		= MAX8698_DVSARM2,
		.ops		= &max8698_regulator_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "DVSARM3",
		.id		= MAX8698_DVSARM3,
		.ops		= &max8698_regulator_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "DVSARM4",
		.id		= MAX8698_DVSARM4,
		.ops		= &max8698_regulator_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "DVSINT1",
		.id		= MAX8698_DVSINT1,
		.ops		= &max8698_regulator_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "DVSINT2",
		.id		= MAX8698_DVSINT2,
		.ops		= &max8698_regulator_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "BUCK3",
		.id		= MAX8698_BUCK3,
		.ops		= &max8698_regulator_ops,
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	},
};

static __devinit int max8698_pmic_probe(struct platform_device *pdev)
{
	struct max8698_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct max8698_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct regulator_dev **rdev;
	struct max8698_data *max8698;
	int i, ret, size;

	if (!pdata) {
		dev_err(pdev->dev.parent, "No platform init data supplied\n");
		return -ENODEV;
	}

	max8698 = kzalloc(sizeof(struct max8698_data), GFP_KERNEL);
	if (!max8698)
		return -ENOMEM;

	size = sizeof(struct regulator_dev *) * pdata->num_regulators;
	max8698->rdev = kzalloc(size, GFP_KERNEL);
	if (!max8698->rdev) {
		kfree(max8698);
		return -ENOMEM;
	}

	rdev = max8698->rdev;
	max8698->dev = &pdev->dev;
	max8698->iodev = iodev;
	max8698->num_regulators = pdata->num_regulators;
	platform_set_drvdata(pdev, max8698);

	for (i = 0; i < pdata->num_regulators; i++) {
		const struct voltage_map_desc *desc;
		int id = pdata->regulators[i].id;

		if (id >= ARRAY_SIZE(regulators))
			continue;

		desc = ldo_voltage_map[id];
		if (desc) {
			int count = (desc->max - desc->min) / desc->step + 1;
			regulators[id].n_voltages = count;
		}
		rdev[i] = regulator_register(&regulators[id], max8698->dev,
				pdata->regulators[i].initdata, max8698);
		if (IS_ERR(rdev[i])) {
			ret = PTR_ERR(rdev[i]);
			dev_err(max8698->dev, "regulator init failed\n");
			rdev[i] = NULL;
			goto err;
		}
	}

	return 0;
err:
	for (i = 0; i < max8698->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	kfree(max8698->rdev);
	kfree(max8698);

	return ret;
}

static int __devexit max8698_pmic_remove(struct platform_device *pdev)
{
	struct max8698_data *max8698 = platform_get_drvdata(pdev);
	struct regulator_dev **rdev = max8698->rdev;
	int i;

	for (i = 0; i < max8698->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	kfree(max8698->rdev);
	kfree(max8698);

	return 0;
}

static struct platform_driver max8698_pmic_driver = {
	.driver = {
		.name = "max8698-pmic",
		.owner = THIS_MODULE,
	},
	.probe = max8698_pmic_probe,
	.remove = max8698_pmic_remove,
};

static int __init max8698_pmic_init(void)
{
	return platform_driver_register(&max8698_pmic_driver);
}
subsys_initcall(max8698_pmic_init);

static void __exit max8698_pmic_exit(void)
{
	platform_driver_unregister(&max8698_pmic_driver);
}
module_exit(max8698_pmic_exit);

MODULE_DESCRIPTION("Maxim 8698 voltage regulator driver");
MODULE_AUTHOR("Tomasz Figa <tomasz.figa at gmail.com>");
MODULE_LICENSE("GPLv2");
