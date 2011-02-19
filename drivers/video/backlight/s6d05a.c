/*
 * drivers/video/backlight/s6d05a.c
 *
 * Copyright (C) 2011 Tomasz Figa <tomasz.figa at gmail.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/leds.h>

#include <video/s6d05a.h>

#include <mach/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/regs-lcd.h>

#include <mach/hardware.h>
#include <mach/spica.h>

#include <mach/gpio.h>

#define BACKLIGHT_MODE_NORMAL	0x000
#define BACKLIGHT_MODE_ALC	0x100
#define BACKLIGHT_MODE_CABC	0x200

#define BACKLIGHT_LEVEL_VALUE	0x0FF

#define BACKLIGHT_LEVEL_MIN	0
#define BACKLIGHT_LEVEL_MAX	BACKLIGHT_LEVEL_VALUE

#define BACKLIGHT_LEVEL_DEFAULT	BACKLIGHT_LEVEL_MAX

/* Driver data */
struct s6d05a_data {
	struct spi_device *spi;

	unsigned reset_gpio;
	void (*set_power)(int);

	int state;
	int brightness;

	struct s6d05a_command *power_on_seq;
	u32 power_on_seq_len;

	struct s6d05a_command *power_off_seq;
	u32 power_off_seq_len;
};

/*
 * Power on command sequence
 */

static struct s6d05a_command s6d05a_power_on_seq[] = {
	{ PWRCTL,  9,  { 0x00, 0x00, 0x2A, 0x00, 0x00, 0x33, 0x29, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   0 },
	{ SLPOUT,  0,  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  15 },

	{ DISCTL,  11, { 0x16, 0x16, 0x0F, 0x0A, 0x05, 0x0A, 0x05, 0x10, 0x00, 0x16, 0x16, 0x00, 0x00, 0x00, 0x00 },   0 },
	{ PWRCTL,  9,  { 0x00, 0x01, 0x2A, 0x00, 0x00, 0x33, 0x29, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   0 },
	{ VCMCTL,  5,  { 0x1A, 0x1A, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   0 },
	{ SRCCTL,  6,  { 0x00, 0x00, 0x0A, 0x01, 0x01, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   0 },
	{ GATECTL, 3,  { 0x44, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  15 },

	{ PWRCTL,  9,  { 0x00, 0x03, 0x2A, 0x00, 0x00, 0x33, 0x29, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  15 },
	{ PWRCTL,  9,  { 0x00, 0x07, 0x2A, 0x00, 0x00, 0x33, 0x29, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  15 },
	{ PWRCTL,  9,  { 0x00, 0x0F, 0x2A, 0x00, 0x02, 0x33, 0x29, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  15 },
	{ PWRCTL,  9,  { 0x00, 0x1F, 0x2A, 0x00, 0x02, 0x33, 0x29, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  15 },
	{ PWRCTL,  9,  { 0x00, 0x3F, 0x2A, 0x00, 0x08, 0x33, 0x29, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  25 },
	{ PWRCTL,  9,  { 0x00, 0x7F, 0x2A, 0x00, 0x08, 0x33, 0x29, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  35 },

	{ MADCTL,  1,  { 0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   0 },
	{ COLMOD,  1,  { 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   0 },
	{ GAMCTL1, 15, { 0x00, 0x00, 0x00, 0x14, 0x27, 0x2D, 0x2C, 0x2D, 0x10, 0x11, 0x10, 0x16, 0x04, 0x22, 0x22 },   0 },
	{ GAMCTL2, 15, { 0x00, 0x00, 0x00, 0x14, 0x27, 0x2D, 0x2C, 0x2D, 0x10, 0x11, 0x10, 0x16, 0x04, 0x22, 0x22 },   0 },
	{ GAMCTL3, 15, { 0x96, 0x00, 0x00, 0x00, 0x00, 0x15, 0x1E, 0x23, 0x16, 0x0D, 0x07, 0x10, 0x00, 0x81, 0x42 },   0 },
	{ GAMCTL4, 15, { 0x80, 0x16, 0x00, 0x00, 0x00, 0x15, 0x1E, 0x23, 0x16, 0x0D, 0x07, 0x10, 0x00, 0x81, 0x42 },   0 },
	{ GAMCTL5, 15, { 0x00, 0x00, 0x34, 0x30, 0x2F, 0x2F, 0x2E, 0x2F, 0x0E, 0x0D, 0x09, 0x0E, 0x00, 0x22, 0x12 },   0 },
	{ GAMCTL6, 15, { 0x00, 0x00, 0x34, 0x30, 0x2F, 0x2F, 0x2E, 0x2F, 0x0E, 0x0D, 0x09, 0x0E, 0x00, 0x22, 0x12 },   0 },
	{ BCMODE,  1,  { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   0 },
	{ MIECTL3, 2,  { 0x7C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   0 },
	{ WRDISBV, 1,  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   0 },
//	{ MIECTL1, 3,  { 0x80, 0x80, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   0 },
//	{ MIECTL2, 3,  { 0x20, 0x01, 0x8F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   0 },
//	{ WRCABC,  1,  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   0 },
	{ DCON,    1,  { 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  40 },

	{ DCON,    1,  { 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   0 },
	{ WRCTRLD, 1,  { 0x2C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   0 },
};

/*
 *	Power off command sequence
 */

static struct s6d05a_command s6d05a_power_off_seq[] = {
	{    DCON,  1, { 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  40 },
	{    DCON,  1, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  25 },
	{  PWRCTL,  9, { 0x00, 0x00, 0x2A, 0x00, 0x00, 0x33, 0x29, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   0 },
	{   SLPIN,  0, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 200 },
};

/*
 *	Hardware interface
 */

static inline void s6d05a_send_command(s6d05a_data *data, s6d05a_command *cmd)
{
	struct spi_message msg;
	u16 buf[16];
	struct spi_transfer xfer = {
		.tx_buf = buf, .rx_buf = 0, .bits_per_word = 9
	};
	int i;
	
	buf[0] = cmd->command;
	for (i = 0; i < cmd->parameters; ++i)
		buf[i + 1] = cmd->parameter[i] | 0x100;

	xfer.len = 2*(cmd->parameters + 1);
	xfer.delay = cmd->wait;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	spi_sync(data->spi, &msg);	
}

static void s6d05a_send_command_seq(s6d05a_data *data,
						s6d05a_command *cmd, int count)
{
	while (--count) 
		s6d05a_send_command(data, cmd++);
}

/*
 *	Backlight interface
 */

static void s6d05a_set_power(struct s6d05a_data *data, int power)
{
	if (power) {
		/* Power On Sequence */

		/* Power Enable */
		data->set_power(1);

		// wait longer than 1ms
		msleep(2);

		/* Make sure reset isn't asserted for at least 1 ms */
		gpio_set_value(data->reset_gpio, 1);
		msleep(1);

		/* Assert reset for at least 1 ms (> 10 us) */
		gpio_set_value(data->reset_gpio, 0);
		msleep(1);

		/* Release reset */
		gpio_set_value(data->reset_gpio, 1);

		/* Wait at least 10 ms */
		msleep(10);

		/* Send power on command sequence */
		s6d05a_send_command_seq(data, data->power_on_seq,
						data->power_on_seq_len);
	} else {
		/* Power Off Sequence */

		/* Send power off command sequence */
		s6d05a_send_command_seq(data, data->power_off_seq,
						data->power_off_seq_len);

		/* Reset Assert */
		gpio_set_value(data->reset_gpio, 0);

		/* Disable power */
		data->set_power(0);
	}

	data->state = value;
}

static void s6d05a_set_backlight(s6d05a_data *data, u8 value)
{
	struct s6d05a_command cmd = { WRDISBV, 1, { value, }, 0 };

	s6d05a_send_command(data, &cmd);
}

static int s6d05a_bl_update_status(struct backlight_device *bl)
{
	struct s6d05a_data *data = bl_get_data(bl);

	data->brightness = bl->props.brightness;

	if (bl->props.state & (BL_CORE_FBBLANK | BL_CORE_SUSPENDED))
	{
		if (data->state)
			s6d05a_set_power(data, 0);
		return 0;
	}

	if (!data->state)
		s6d05a_set_power(data, 1);

	s6d05a_set_backlight(data, data->brightness);

	return 0;
}

static int s6d05a_bl_get_brightness(struct backlight_device *bl)
{
	struct s6d05a_data *data = bl_get_data(bl);

	if (!data->state)
		return 0;

	return data->brightness;
}

static struct backlight_ops s6d05a_bl_ops = {
	.update_status	= s6d05a_bl_update_status,
	.get_brightness	= s6d05a_bl_get_brightness,
};

static struct backlight_properties s6d05a_bl_props = {
	.max_brightness	= BACKLIGHT_LEVEL_MAX,
};

/*
 * SPI driver
 */

static int __devinit s6d05a_probe(struct spi_device *spi)
{
	struct s6d05a_data *data;
	struct s6d05a_pdata *pdata = spi->dev.platform_data;
	struct lcd_device *ld;
	int ret = 0;

	if (!pdata || !pdata->set_power)
		return -ENOENT;

	/* Configure GPIO */
	ret = gpio_request(pdata->reset_gpio, "s6d05a reset");
	if (ret)
		return ret;

	ret = gpio_direction_output(pdata->reset_gpio, 1);
	if (ret)
		goto err;

	/* Allocate memory for driver data */
	st = kzalloc(sizeof(struct s6d05a_data), GFP_KERNEL);
	if (st == NULL) {
		dev_err(&spi->dev, "No memory for device state\n");
		ret = -ENOMEM;
		goto err;
	}

	/* Register backlight device */
	bl = backlight_device_register(dev_driver_string(&spi->dev),
			&spi->dev, data, &s6d05a_bl_ops, &s6d05a_bl_props);
	if (IS_ERR(bl)) {
		dev_err(&client->dev, "Failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err2;
	}

	/* Set up driver data */
	data->set_power = pdata->set_power;
	data->reset_gpio = pdata->reset_gpio;
	data->spi = spi;
	data->bl = bl;
	data->power_on_seq = s6d05a_power_on_seq;
	data->power_on_seq_len = ARRAY_SIZE(s6d05a_power_on_seq);
	data->power_off_seq = s6d05a_power_off_seq;
	data->power_off_seq_len = ARRAY_SIZE(s6d05a_power_off_seq);

	/* Check for power on sequence override */
	if (pdata->power_on_seq) {
		data->power_on_seq = pdata->power_on_seq;
		data->power_on_seq_len = pdata->power_on_seq_len;
	}

	/* Check for power off sequence override */
	if (pdata->power_off_seq) {
		data->power_off_seq = pdata->power_off_seq;
		data->power_off_seq_len = pdata->power_off_seq_len;
	}

	dev_set_drvdata(&spi->dev, data);

	/* Initialize the LCD */
	s6d05a_set_power(data, 1);
	s6d05a_set_backlight(data, BACKLIGHT_LEVEL_MAX);

	return 0;

err2:
	kfree(data);
err:
	gpio_free(pdata->reset_gpio);

	return ret;
}

static int __devexit s6d05a_remove(struct spi_device *spi)
{
	struct s6d05a_data *data = dev_get_drvdata(&spi->dev);

	s6d05a_set_power(data, 0);
	backlight_device_unregister(data->bl);
	gpio_free(data->reset_gpio);
	kfree(data);

	return 0;
}

static struct spi_driver s6d05a_driver = {
	.driver = {
		.name	= "lcd_s6d05a",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
	.probe		= s6d05a_probe,
	.remove		= s6d05a_remove,
	.suspend	= s6d05a_suspend,
	.resume		= s6d05a_resume,
};

/*
 * Module
 */

static int __init s6d05a_init(void)
{
	return spi_register_driver(&s6d05a_driver);
}
module_init(s6d05a_init);

static void __exit s6d05a_exit(void)
{
	spi_unregister_driver(&s6d05a_driver);
}
module_exit(s6d05a_exit);

MODULE_DESCRIPTION("S6D05A LCD Controller Driver");
MODULE_AUTHOR("Tomasz Figa <tomasz.figa at gmail.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:s6d05a");
