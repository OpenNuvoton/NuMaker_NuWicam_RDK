/*
 *
 * Copyright (C) 2009 Nuvoton corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation;version 2 of the License.
 * For NuWicam boards
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/leds.h>

/* GPIO group definition */
#define GPIO_GROUP_A 0
#define GPIO_GROUP_B 1
#define GPIO_GROUP_C 2
#define GPIO_GROUP_D 3
#define GPIO_GROUP_E 4
#define GPIO_GROUP_G 5
#define GPIO_GROUP_H 6

#define GPIO_IDX_NUM(A,B) 	(A*32+B)

/* Leds */
/* Define LEDS function in here */
static struct gpio_led board_leds[] = {
	[0] = {
		.name		 	= "READY",	/* any string */
		.default_trigger 	= "heartbeat",	/* none, nand-disk, mmc0, mmc1, timer, heartbeat, backlight, gpio, default-on */
		.gpio		 	= GPIO_IDX_NUM(GPIO_GROUP_B, 4),  /* Pin */
		.active_low	 	= 1,
	},
	[1] = {
		.name		 	= "LINK",
		.default_trigger 	= "mmc0",
		.gpio		 	= GPIO_IDX_NUM(GPIO_GROUP_B, 5),
		.active_low	 	= 1,
	},
	[2] = {
		.name		 	= "Reserve",
		.default_trigger 	= "gpio",
		.gpio		 	= GPIO_IDX_NUM(GPIO_GROUP_B, 6),
		.active_low	 	= 1,
	}
};

static struct gpio_led_platform_data board_leds_info = {
	.leds		= board_leds,
	.num_leds	= ARRAY_SIZE(board_leds),
};

static struct platform_device board_leds_device = {
	.name		= "leds-gpio",
	.id		= -1,
	.dev		= {
		.platform_data	= &board_leds_info,
	},
};

static int __init board_leds_init(void)
{
	int i=0;
	for ( i=0; i<ARRAY_SIZE(board_leds); i++ )
		printk("Set <%s> to [%s], Pin: GP%c%d\n", board_leds[i].name, board_leds[i].default_trigger, (board_leds[i].gpio/32+65) ,board_leds[i].gpio%32 );
	return platform_device_register(&board_leds_device);
}

module_init(board_leds_init);

MODULE_AUTHOR("Wayne Lin");
MODULE_DESCRIPTION("BOARD-LEDS driver");
MODULE_LICENSE("GPL");
