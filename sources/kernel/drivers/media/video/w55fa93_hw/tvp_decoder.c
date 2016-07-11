#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/io.h>
#include <mach/w55fa93_reg.h>
#include <mach/w55fa93_gpio.h>

#include "tvp_decoder.h"

//For NuWicam board
E_TVP_MODE tvp_getmode(void)
{
	E_TVP_MODE ret=0;
	
	/* set share pin and direction of gpios */
	w55fa93_gpio_configure(GPIO_GROUP_B, 0);
	// Set GPB0 as GPIO input mode
	w55fa93_gpio_set_input(GPIO_GROUP_B, 0);
	writel ( readl(REG_GPIOB_PUEN) | BIT0, REG_GPIOB_PUEN ); 
	/* returns the value of the GPIO pin */
	ret = (E_TVP_MODE)w55fa93_gpio_get(GPIO_GROUP_B, 0);	
	printk("[%s %d] GPB0 pin status is %d\n", __FUNCTION__, __LINE__, ret );
	g_eTVPMode = ret;
	
	switch ( g_eTVPMode )
	{
		case eTVP_NTSC:
			CROP_START_X = CONFIG_NTSC_CROP_START_X;
			CROP_START_Y = CONFIG_NTSC_CROP_START_Y;
			break;
				
		case eTVP_PAL:
		default:
			CROP_START_X = CONFIG_PAL_CROP_START_X;
			CROP_START_Y = CONFIG_PAL_CROP_START_Y;
			break;
	}	
	return ret;
}
