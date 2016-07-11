(1) Built-in CONFIG_GPIO_W55FA92, CONFIG_NEW_LEDS, CONFIG_LEDS_CLASS, CONFIG_LEDS_GPIO, CONFIG_LEDS_GPIO_PLATFORM,
    CONFIG_LEDS_TRIGGERS, CONFIG_LEDS_TRIGGER_TIMER, CONFIG_LEDS_TRIGGER_HEARTBEAT, 
    CONFIG_LEDS_TRIGGER_BACKLIGHT, CONFIG_LEDS_TRIGGER_GPIO and CONFIG_LEDS_TRIGGER_DEFAULT_ON option
    into conprog.bin.

(2) If you need use this driver, you must modify variable in Makefile
	KERNELSRC := <PATH/TO/YOUR/KERNEL/SOURCE>
	Ex:
	KERNELSRC := /home/wclin/workspace/PRJ/bsp/bsp_w55fa92/linux-2.6.35.4_fa92
    
(3) Execute 'make' to build driver.

(4) Insert the board_leds.ko kernel module to archive functions.

(5) Enjoy
