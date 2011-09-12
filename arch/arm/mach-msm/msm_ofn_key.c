/*add code to support JOY(U8120) */

/* drivers/input/mouse/ofn_key.c
 *
 * Copyright (C) 2009 HUAWEI.
 **
 */


#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>
#include "linux/hardware_self_adapt.h"
#include <linux/irq.h>

//#define TS_DEBUG 
//#undef TS_DEBUG 

#ifdef TS_DEBUG
#define OK_KEY_DEBUG(fmt, args...) printk(KERN_ERR fmt, ##args)
#else
#define OK_KEY_DEBUG(fmt, args...)
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif


#define OFN_GPIO_OK                             40
#define OFN_GPIO_OK_U8500                             39

#define OFN_OK_KEY_INT_DELAY                    50000000

struct ofn_ok_key_data {
    struct input_dev *input_dev;
    uint16_t ok_key_gpio;
    int  ok_key_irq;
    bool ok_key_press;
    struct hrtimer timer;	
};

static enum hrtimer_restart ofn_ok_key_timer_func(struct hrtimer *timer)
{
	struct ofn_ok_key_data *data = container_of(timer, struct ofn_ok_key_data, timer);

    if(!gpio_get_value(data->ok_key_gpio))
    {
    
    	set_irq_type(data->ok_key_irq, IRQF_TRIGGER_RISING);  
        if(FALSE == data->ok_key_press)
        {
            /*report OK key press*/
            OK_KEY_DEBUG("ofn_ok_irq_handler: key_pressed\n");
            data->ok_key_press = TRUE;
            
            input_report_key(data->input_dev, KEY_REPLY, !!data->ok_key_press);
            input_sync(data->input_dev);
        }

    }
    else
    {
    	set_irq_type(data->ok_key_irq, IRQF_TRIGGER_FALLING);  
        /*report OK key release*/
        if(TRUE == data->ok_key_press)
        {
            OK_KEY_DEBUG("ok_key_timer_func: key_release\n");
            data->ok_key_press = FALSE;

            input_report_key(data->input_dev, KEY_REPLY, !!data->ok_key_press);
            input_sync(data->input_dev);
        }
    }


    enable_irq(data->ok_key_irq);
    
	return HRTIMER_NORESTART;

}


static irqreturn_t ofn_ok_irq_handler(int irq, void *dev_id)
{
	struct ofn_ok_key_data *data = dev_id;

	if (!data->ok_key_irq) /* ignore interrupt while registering the handler */
		return IRQ_HANDLED;

    disable_irq_nosync(data->ok_key_irq);

	hrtimer_start(&data->timer, ktime_set(0, OFN_OK_KEY_INT_DELAY), HRTIMER_MODE_REL);

	return IRQ_HANDLED;
}

static int msm_ofn_ok_key_probe(struct platform_device *pdev)
{
    int ret = 0;
    int gpio_config;
    struct ofn_ok_key_data *data;
    bool msm_ofn_support = false;
    
    board_support_ofn(&msm_ofn_support);
    if(false == msm_ofn_support)
    {
        /*this board don't support OFN, and don't need OFN key */
        return 0;
    }
    
	data = kzalloc(sizeof(struct ofn_ok_key_data) ,GFP_KERNEL);
	if (!data)
    	return -ENOMEM;
    	if(machine_is_msm7x25_u8500() || machine_is_msm7x25_um840())
    	{
    	  data->ok_key_gpio = OFN_GPIO_OK_U8500;
    	}
    	else
    	{
    	  data->ok_key_gpio = OFN_GPIO_OK;
    	}

	OK_KEY_DEBUG("msm_ofn_ok_key_probe: in probe\n");

	data->input_dev = input_allocate_device();
	if (NULL == data->input_dev) {
		ret = -ENOMEM;
		OK_KEY_DEBUG(KERN_ERR "msm_ofn_ok_key_probe: Failed to allocate input device \n");
		goto err_input_dev_alloc_failed;
	}

	data->input_dev->name = "ofn_ok_key";

	ret = input_register_device(data->input_dev);
	if (ret) {
		OK_KEY_DEBUG(KERN_ERR "msm_ofn_ok_key_probe: Unable to register %s input device\n", data->input_dev->name);
		goto err_input_register_device_failed;
	}   

    /*ok keypad value 232*/
	input_set_capability(data->input_dev, EV_KEY, KEY_REPLY);

	//initial timer and register device before enable interrupt.
    hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    data->timer.function = ofn_ok_key_timer_func;

	platform_set_drvdata(pdev, data);

	gpio_config = GPIO_CFG(data->ok_key_gpio, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA);
	ret = gpio_tlmm_config(gpio_config, GPIO_ENABLE);    
	OK_KEY_DEBUG("msm_ofn_ok_key_probe: gpio_config ret=%d\n", ret);
    
	if (ret) 
		return -EIO;

    data->ok_key_press = FALSE;
        
    data->ok_key_irq = gpio_to_irq(data->ok_key_gpio);

	set_irq_type(data->ok_key_irq, IRQF_TRIGGER_FALLING);  
	
	ret = request_irq(data->ok_key_irq, ofn_ok_irq_handler, 0, "avago_ok_key", data);

	if (ret != 0) {
		free_irq(data->ok_key_irq, data);
		
		printk(KERN_ERR "msm_ofn_ok_key_probe: enable OFN OK IRQ failedd\n");
		goto err_input_register_device_failed;

	}
		OK_KEY_DEBUG("msm_ofn_ok_key_probe: OK ,ok_key-irq = %d\n", data->ok_key_irq);

		//delete 3 line
    return 0;

err_input_register_device_failed:
    input_free_device(data->input_dev);
err_input_dev_alloc_failed:
	kfree(data);
    return ret;
}


static int msm_ofn_ok_key_remove(struct platform_device *pdev)
{
	struct ofn_ok_key_data *data = platform_get_drvdata(pdev);

	free_irq(data->ok_key_irq, data);
    input_unregister_device(data->input_dev);
    kfree(data);

    return 0;
}


static struct platform_device msm_ofn_ok_dev = {
	.name		= "ofn_ok_key",
	.id		= -1,

};



static struct platform_driver msm_ofn_ok_key_drv = {
	.probe		= msm_ofn_ok_key_probe,
	.remove		= msm_ofn_ok_key_remove,
	.driver		= {
		.name		= "ofn_ok_key",
	},
};

int init_ofn_ok_key_device(void)
{
   return platform_device_register(&msm_ofn_ok_dev);
}

static int __init ofn_ok_key_init(void)
{
	
	return platform_driver_register(&msm_ofn_ok_key_drv);
}

static void __exit ofn_ok_key_exit(void)
{
	platform_driver_unregister(&msm_ofn_ok_key_drv);
}

late_initcall(ofn_ok_key_init);
module_exit(ofn_ok_key_exit);

MODULE_DESCRIPTION("OFN Single OK key Driver");
MODULE_LICENSE("GPL");

