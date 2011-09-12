/*
 * Driver for jogball on GPIO lines capable of generating interrupts.
 *
 * Copyright 2009 l63336
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/jogball_driver.h>
#include <asm/gpio.h>
#include <linux/irqreturn.h>
#include <linux/workqueue.h>

//#define JOGBALL_RIGHT  2

static DEFINE_MUTEX(up_mutex);
static DEFINE_MUTEX(down_mutex);
static DEFINE_MUTEX(right_mutex);
static DEFINE_MUTEX(left_mutex);

enum {
    JOGBALL_UP,
    JOGBALL_DOWN,
    JOGBALL_LEFT,
    JOGBALL_RIGHT
};

static int g_irq[4];

extern int g_iJogballReport;
static atomic_t reportcounts = ATOMIC_INIT(0);

/*used atomic for security*/
static atomic_t dwscrollcounts[4] = {ATOMIC_INIT(0),ATOMIC_INIT(0),ATOMIC_INIT(0),ATOMIC_INIT(0)};

irqreturn_t jogball_isr(int irq, void *dev_id)
{
    struct jogball_drvdata *pdata = (struct jogball_drvdata *)dev_id;   
    disable_irq(gpio_to_irq(irq));
/*we remember pos here*/
    if(g_irq[JOGBALL_UP] == irq)
    {
//        atomic_inc(&dwscrollcounts[JOGBALL_UP]);
        atomic_set(&dwscrollcounts[JOGBALL_UP],1);
    }
    else if(g_irq[JOGBALL_DOWN] == irq)
    {
//        atomic_inc(&dwscrollcounts[JOGBALL_DOWN]);
        atomic_set(&dwscrollcounts[JOGBALL_DOWN],1);
    }
    else if(g_irq[JOGBALL_LEFT] == irq)
    {
//        atomic_inc(&dwscrollcounts[JOGBALL_LEFT]);
        atomic_set(&dwscrollcounts[JOGBALL_LEFT],1);
    }
    else if(g_irq[JOGBALL_RIGHT] == irq)
    {
//        atomic_inc(&dwscrollcounts[JOGBALL_RIGHT];
        atomic_set(&dwscrollcounts[JOGBALL_RIGHT],1);
    }
    else
    {
        if((g_irq[JOGBALL_RIGHT] == 0) || (g_irq[JOGBALL_LEFT] == 0) || (g_irq[JOGBALL_DOWN] == 0) || (g_irq[JOGBALL_UP] == 0))
        {
            //first time error.we do nothing here
            ;
        }
        else
        {
            printk("\n jogball get irq error!\n");        
        }
        enable_irq(gpio_to_irq(irq));
        return IRQ_HANDLED;        
    }
    schedule_work(&pdata->jogball_work);    
    enable_irq(gpio_to_irq(irq));
    return IRQ_HANDLED;
}   

/*request irs*/
int jogball_up_isr_start(struct platform_device *pdev)
{  
	struct jogball_platform_data *pdata = pdev->dev.platform_data; 
	struct jogball_drvdata *pirqdata = platform_get_drvdata(pdev); 
	struct jogball_button *button = &pdata->buttons[0];
	int irq, error;
   
	irq = gpio_to_irq(button->gpio);
	set_irq_type(irq, IRQF_TRIGGER_RISING);  
	error = request_irq(irq, (irq_handler_t)jogball_isr, 0, "jogball_up", pirqdata);
	if (error) {
		printk("jogball: Unable to claim irq %d; error %d\n", irq, error);
              free_irq(gpio_to_irq(button->gpio), pdev);
		gpio_free(button->gpio);
		return error;		
	}
	g_irq[JOGBALL_UP] = irq;
	return 0;
}

int jogball_down_isr_start(struct platform_device *pdev)
{  
	struct jogball_platform_data *pdata = pdev->dev.platform_data; 
	struct jogball_drvdata *pirqdata = platform_get_drvdata(pdev); 
	struct jogball_button *button = &pdata->buttons[1];
	int irq, error;
   
	irq = gpio_to_irq(button->gpio);	
	set_irq_type(irq, IRQF_TRIGGER_RISING);  
	error = request_irq(irq, jogball_isr, 0, "jogball_down", pirqdata);
	if (error) {
		printk("jogball: Unable to claim irq %d; error %d\n", irq, error);
              free_irq(gpio_to_irq(button->gpio), pdev);
		gpio_free(button->gpio);		
		return error;		
	}
	g_irq[JOGBALL_DOWN] = irq;	
	return 0;
 }

int jogball_left_isr_start(struct platform_device *pdev)
{  
	struct jogball_platform_data *pdata = pdev->dev.platform_data; 
	struct jogball_drvdata *pirqdata = platform_get_drvdata(pdev);
	struct jogball_button *button = &pdata->buttons[2];
	int irq, error;
 
	irq = gpio_to_irq(button->gpio);	
	set_irq_type(irq, IRQF_TRIGGER_RISING);  
       error = request_irq(irq, jogball_isr, 0, "jogball_left", pirqdata);
	if (error) {
		printk("jogball: Unable to claim irq %d; error %d\n", irq, error);
              free_irq(gpio_to_irq(button->gpio), pdev);
		gpio_free(button->gpio);
		return error;
	}
	g_irq[JOGBALL_LEFT] = irq;	
	return 0;
}

int jogball_right_isr_start(struct platform_device *pdev)
{  
	struct jogball_platform_data *pdata = pdev->dev.platform_data; 
	struct jogball_drvdata *pirqdata = platform_get_drvdata(pdev);
	struct jogball_button *button = &pdata->buttons[3];
	int irq, error;

	irq = gpio_to_irq(button->gpio);	
	set_irq_type(irq, IRQF_TRIGGER_RISING);  
	error = request_irq(irq, jogball_isr, 0, "jogball_right", pirqdata);
	if (error) {
		printk("jogball: Unable to claim irq %d; error %d\n", irq, error);
              free_irq(gpio_to_irq(button->gpio), pdev);
		gpio_free(button->gpio);
		return error;		
	}
	g_irq[JOGBALL_RIGHT] = irq;	
	return 0;
}

void jogball_do_work(struct work_struct *work)
{
    int rel_x = 0;
    int rel_y = 0;
    int up,down,left,right;
    struct jogball_drvdata *ddata = container_of(work, struct jogball_drvdata, jogball_work);

    up = atomic_read(&dwscrollcounts[JOGBALL_UP]);
    down = atomic_read(&dwscrollcounts[JOGBALL_DOWN]);
    left = atomic_read(&dwscrollcounts[JOGBALL_LEFT]);
    right = atomic_read(&dwscrollcounts[JOGBALL_RIGHT]);
    atomic_inc(&reportcounts);
    
    if (up > down)
    {
        rel_y = - up;
    }
    else
    {
        rel_y = down;
    }

    if (left > right)
    {
        rel_x = - left;
    }
    else
    {
        rel_x = right;
    }
    /*we set 0 first because of the report opt take more time*/ 	
    atomic_set(&dwscrollcounts[JOGBALL_UP],0);
    atomic_set(&dwscrollcounts[JOGBALL_DOWN],0);
    atomic_set(&dwscrollcounts[JOGBALL_LEFT],0);
    atomic_set(&dwscrollcounts[JOGBALL_RIGHT],0);
    if(atomic_read(&reportcounts) >= g_iJogballReport)
    {
    
/*for security .we should decide the value of rel_x,rel_y better.*/
//    input_report_key(ddata->input, BTN_MOUSE, 1);
        /*delete some lines*/
        input_report_rel(ddata->input, REL_X, rel_x);
        input_report_rel(ddata->input, REL_Y, rel_y);
    input_sync(ddata->input);
    atomic_set(&reportcounts,0);
    }
    return;
}
