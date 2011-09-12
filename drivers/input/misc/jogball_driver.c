/*
 * Driver for jogball on GPIO lines capable of generating interrupts.
 *
 * Copyright 2009 l63336
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/jogball_driver.h>
#include <asm/gpio.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#define JOGBALL_REPNUM 2

int g_iJogballReport = 0;

#ifdef CONFIG_HUAWEI_FEATURE_U8220_PP1
/*
0 : T1,T2
1 : PP1
2 : not used
*/
#define HW_VER_T1       0
#define HW_VER_T2       0
#define HW_VER_PP1     1  
/*we use this to deal jogball by hardware*/
static u8 bio_get_hw_sub_ver(void)
{
     u32 u_gpio_20_val;

     u32 u_gpio_101_val;

     u32 u_hw_sub_version;

     // GPIO 20 
     gpio_tlmm_config(GPIO_CFG(20,0,GPIO_INPUT,GPIO_PULL_UP,GPIO_2MA), GPIO_ENABLE);

     gpio_tlmm_config(GPIO_CFG(101,0,GPIO_INPUT,GPIO_PULL_UP,GPIO_2MA), GPIO_ENABLE);

     u_gpio_20_val = gpio_get_value(20);

     u_gpio_101_val = gpio_get_value(101);

     if((u_gpio_20_val == 0)&&(u_gpio_101_val == 0))
     {
         u_hw_sub_version = 0;
     }
     else if((u_gpio_20_val != 0)&&(u_gpio_101_val == 0))
     {
         u_hw_sub_version = 1;
     }
	 else if((u_gpio_20_val == 0)&&(u_gpio_101_val != 0))
	 {
        u_hw_sub_version = 2;
     }
	 else
	 {
	     u_hw_sub_version = 3;
	 }

     gpio_tlmm_config(GPIO_CFG(20,0,GPIO_INPUT,GPIO_PULL_DOWN,GPIO_2MA), GPIO_ENABLE);

     gpio_tlmm_config(GPIO_CFG(101,0,GPIO_INPUT,GPIO_PULL_DOWN,GPIO_2MA), GPIO_ENABLE);

     return u_hw_sub_version;
}
#endif

static int __devinit jogball_probe(struct platform_device *pdev)
{
    struct jogball_platform_data *pdata = pdev->dev.platform_data;
    struct jogball_drvdata *ddata;
    struct jogball_button *button;
    int i;
    int error = -1;

#ifdef CONFIG_HUAWEI_FEATURE_U8220_PP1
    int iHwversion = -1;
#endif

    gpio_tlmm_config(GPIO_CFG(pdata->gpio_ctl, 0, GPIO_OUTPUT,  GPIO_PULL_UP, GPIO_2MA), GPIO_ENABLE);
    for (i = 0; i < pdata->nbuttons; i++) {
        button = &pdata->buttons[i];
        gpio_tlmm_config(GPIO_CFG(button->gpio, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
    }

    ddata = kzalloc(sizeof(struct jogball_drvdata) + pdata->nbuttons * sizeof(struct gpio_button_data), GFP_KERNEL);
    if (!ddata) {
        error = -ENOMEM;
        printk("\n kzalloc error \n");        
        goto err_alloc;
    }

    ddata->input  = input_allocate_device();
    if (!(ddata->input)) {
        error = -ENOMEM;
        printk("\n input_allocate_device error \n");        
        goto err_inputalloc;
    }
    
    set_bit(EV_KEY, ddata->input->evbit);
    platform_set_drvdata(pdev, ddata);

    ddata->input->name = pdev->name;
    error = gpio_request(pdata->gpio_ctl, "jogball_ctl");
    if (error<0) {
        printk("jogball_ctl: failed to request GPIO for jogball_ctl %d,"
        " error %d\n", pdata->gpio_ctl, error);
        goto err_gpio_req;
        
    }
    error = gpio_direction_output(pdata->gpio_ctl,GPIO_LOW_VALUE);
    if (error<0) {
        pr_err("jogball_clt: gpio_configure failed for "
        "output %d\n", pdata->gpio_ctl);
        goto err_gpio_dir;
    }      

    for (i = 0; i < pdata->nbuttons; i++) {
        button = &pdata->buttons[i];
        //unsigned int type = button->type ?: EV_KEY;                
        error = gpio_request(button->gpio, "jogball");
        if (error < 0) {
            printk("jogball: failed to request GPIO %d,"
            " error %d\n", button->gpio, error);
            while (--i >= 0) {
                gpio_free(pdata->buttons[i].gpio);
            }            
            goto err_gpio_dir;
        }
        error = gpio_direction_input(button->gpio);
        if (error < 0) {
            printk("jogball: failed to configure input"
            " direction for GPIO %d, error %d\n",
            button->gpio, error);
            while (--i >= 0) {
                gpio_free(pdata->buttons[i].gpio);
            }
            /*we release one more*/
            gpio_free(button->gpio);
            goto err_gpio_dir;
        }

    //input_set_capability(ddata->input, type, button->code);
    }

#ifdef CONFIG_HUAWEI_FEATURE_U8220_PP1
/*decide HW version for deal jogball */
    iHwversion = bio_get_hw_sub_ver();

    if(HW_VER_T1 == iHwversion)
    {/*T1,T2*/
        g_iJogballReport = 0;
    }
    else if (HW_VER_PP1 == iHwversion)
    {/*PP1*/
        g_iJogballReport = JOGBALL_REPNUM;
    }
    else
    {
        g_iJogballReport = JOGBALL_REPNUM;
    }
#endif

    g_iJogballReport = JOGBALL_REPNUM;
    input_set_capability(ddata->input, EV_KEY, BTN_MOUSE);
    input_set_capability(ddata->input, EV_REL, REL_X);
    input_set_capability(ddata->input, EV_REL, REL_Y);
    
    INIT_WORK(&ddata->jogball_work, jogball_do_work);

    error = jogball_up_isr_start(pdev);
    if(error < 0)
    {
        goto err_gpiodir_req;
    }
    error = jogball_down_isr_start(pdev);
    if(error < 0)
    {
        goto err_gpiodir_req;
    }
    error = jogball_left_isr_start(pdev);
    if(error < 0)
    {
        goto err_gpiodir_req;
    }
    error = jogball_right_isr_start(pdev);
    if(error < 0)
    {
        goto err_gpiodir_req;
    }

    error = input_register_device(ddata->input);
    if (error) {
        printk("jogball: Unable to register input device, " "error: %d\n", error);
        goto err_gpiodir_req;
    }
    return 0;
//error:
err_gpiodir_req:
    for(i = 0; i < pdata->nbuttons; i++) {
        gpio_free(pdata->buttons[i].gpio);
    }
err_gpio_dir:
    //gpio_free(JOGBALL_CTL);
    gpio_free(pdata->gpio_ctl);
err_gpio_req:
    platform_set_drvdata(pdev, NULL);
    input_free_device(ddata->input);
err_inputalloc:
    kfree(ddata);
err_alloc:
    return error;
}

static int __devexit jogball_remove(struct platform_device *pdev)
{
    struct jogball_platform_data *pdata = pdev->dev.platform_data;
    struct jogball_drvdata *ddata = platform_get_drvdata(pdev);
    int i;

    platform_set_drvdata(pdev, NULL);    
    device_init_wakeup(&pdev->dev, 0);
    for (i = 0; i < pdata->nbuttons; i++) {
        int irq = gpio_to_irq(pdata->buttons[i].gpio);
        free_irq(irq, pdev);
        gpio_free(pdata->buttons[i].gpio);
    }
    
    //gpio_direction_output(JOGBALL_CTL,GPIO_HIGH_VALUE);
    gpio_direction_output(pdata->gpio_ctl,GPIO_HIGH_VALUE);
    input_unregister_device(ddata->input);
    input_free_device(ddata->input);    
    kfree(ddata);
    return 0;
}


static int jogball_suspend(struct platform_device* pdev, pm_message_t state)
{
    struct jogball_platform_data *pdata = pdev->dev.platform_data;
    int i;

    for ( i = 0; i < pdata->nbuttons; i++) {
        int irq = gpio_to_irq(pdata->buttons[i].gpio);
        disable_irq(irq);
    }

    gpio_direction_output(pdata->gpio_ctl,GPIO_HIGH_VALUE);
#if 0
    disable_irq(gpio_to_irq(GPIO_UP));
    disable_irq(gpio_to_irq(GPIO_DOWN));
    disable_irq(gpio_to_irq(GPIO_LEFT));
    disable_irq(gpio_to_irq(GPIO_RIGHT));
    gpio_direction_output(JOGBALL_CTL,GPIO_HIGH_VALUE);
#endif
    return 0;
}

static int jogball_resume(struct platform_device* pdev)
{
    struct jogball_platform_data *pdata = pdev->dev.platform_data;
    int i;

    gpio_direction_output(pdata->gpio_ctl,GPIO_LOW_VALUE);

    for ( i = 0; i < pdata->nbuttons; i++) {
        int irq = gpio_to_irq(pdata->buttons[i].gpio);
        enable_irq(irq);
    }
#if 0
    gpio_direction_output(JOGBALL_CTL,GPIO_LOW_VALUE);
    enable_irq(gpio_to_irq(GPIO_UP));
    enable_irq(gpio_to_irq(GPIO_DOWN));
    enable_irq(gpio_to_irq(GPIO_LEFT));
    enable_irq(gpio_to_irq(GPIO_RIGHT));
#endif
    return 0;
    
}

static struct platform_driver jogball_device_driver = {
	.probe		= jogball_probe,
	.remove		= jogball_remove,
	.suspend	= jogball_suspend,
       .resume		= jogball_resume,
	.driver		= {
		.name	= "jogball",
		.owner	= THIS_MODULE,
	}
};

static int __init jogball_init(void)
{
	return platform_driver_register(&jogball_device_driver);
}

static void __exit jogball_exit(void)
{
	platform_driver_unregister(&jogball_device_driver);
}

module_init(jogball_init);
module_exit(jogball_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cyy134456");
MODULE_DESCRIPTION("Jogball driver for GPIOs");
MODULE_ALIAS("platform:jogball");

