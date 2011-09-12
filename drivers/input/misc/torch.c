/* drivers/input/misc/torch.c - torch driver which use input handler to turn on/off torch
 * 
 * Copyright (C) 2007-2010 Huawei.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <mach/camera.h>
#include <mach/pmic.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/kthread.h>
#include <asm/processor.h>
#include <linux/err.h>
#include <linux/wakelock.h>
#ifdef CONFIG_HUAWEI_EVALUATE_POWER_CONSUMPTION
#include <mach/huawei_battery.h>
#define  CAMERA_FLASH_CUR_DIV 	10
#endif

#define DEBUG_TORCH
//#undef DEBUG_TORCH

#ifdef DEBUG_TORCH
#define TORCH_DEBUG(fmt, args...) printk(KERN_ERR fmt, ##args)
#else
#define TORCH_DEBUG(fmt, args...)
#endif

#define KEY_TIME_SECS		1
#define LIGHT_TIME_SECS		45

#define TORCH_ON_CURRENT	100
#define TORCH_OFF_CURRENT	0

#define KEY_PRESS		1
#define KEY_RELEASE		0

#define KEY_TORCH 		KEY_F10

enum torch_state
{
	TORCH_STATE_MIN,
	TORCH_DISABLE_STATE,
	TORCH_OFF_STATE,
	TORCH_ON_STATE,
	TORCH_STATE_MAX,
};

enum torch_event
{
	TORCH_EVENT_MIN,
	SHORT_PRESS_EVENT,
	LONG_PRESS_EVENT,
	TORCH_DISABLE_EVENT,
	TORCH_ENABLE_EVENT,
	TORCH_TIMEOUT_EVENT,
	TORCH_EVENT_MAX,
};

struct torch_fsm_entry
{
	enum torch_state init;
	enum torch_state final;
	enum torch_event event;
	void (* callb)(struct torch_fsm_entry *event);
};

struct torch_fsm_data 
{
	struct timer_list torch_timer;
	wait_queue_head_t wait_queue;
	struct task_struct *task;

	struct mutex lock;
	struct list_head list;
	enum torch_state state;
};

struct torch_event_data 
{
	struct list_head list;
	enum torch_event event;
};

enum key_state
{
	KEY_STATE_MIN,
	KEY_IDLE_STATE,
	KEY_PRESSED_STATE,
	KEY_TIMEOUT_STATE,
	KEY_STATE_MAX,
};

enum key_event
{
	KEY_EVENT_MIN,
	KEY_PRESS_EVENT,
	KEY_RELEASE_EVENT,
	KEY_TIMEOUT_EVENT,
	KEY_EVENT_MAX,
};

struct key_fsm_entry
{
	enum key_state init;
	enum key_state final;
	enum key_event event;
	void (* callb)(struct key_fsm_entry *event);
};

struct key_fsm_data 
{
  	struct timer_list key_timer;
	wait_queue_head_t wait_queue;
	struct task_struct *task;

	struct mutex lock;
	struct list_head list;
	enum key_state state;
};

struct key_event_data
{
	struct list_head list;
	enum key_event event;
};

static struct wake_lock torch_wake_lock;

static struct torch_fsm_data *torch_fsm = NULL;
static struct key_fsm_data *key_fsm = NULL;

static struct task_struct* torch_fsm_thread_start(void);
static void torch_fsm_thread_exit(void);
static int torch_fsm_thread(void *arg);
static struct torch_event_data* torch_new_event(enum torch_event event);
static int torch_fsm_add_event(struct torch_event_data *event);

static void torch_timer_init(void);
static void torch_timer_start(void);
static int  torch_timer_cancel(void);
static void torch_timer_func(unsigned long arg);

static void torch_fsm_call_event(enum torch_state c_state, enum torch_event c_event);

static void cb_torch_1(struct torch_fsm_entry *entry);
static void cb_torch_2(struct torch_fsm_entry *entry);
static void cb_torch_3(struct torch_fsm_entry *entry);
static void cb_torch_4(struct torch_fsm_entry *entry);
static void cb_torch_5(struct torch_fsm_entry *entry);
static void cb_torch_6(struct torch_fsm_entry *entry);
static void cb_torch_7(struct torch_fsm_entry *entry);


static struct task_struct* key_fsm_thread_start(void);
static void key_fsm_thread_exit(void);
static int key_fsm_thread(void *arg);
static struct key_event_data* key_new_event(enum key_event event);
static int key_fsm_add_event(struct key_event_data *event);

static void key_timer_init(void);
static void key_timer_start(void);
static int  key_timer_cancel(void);
static void key_timer_func(unsigned long arg);

static void cb_key_1(struct key_fsm_entry *entry);
static void cb_key_2(struct key_fsm_entry *entry);
static void cb_key_3(struct key_fsm_entry *entry);
static void cb_key_4(struct key_fsm_entry *entry);

static void key_fsm_call_event(enum key_state c_state, enum key_event c_event);

static void torch_on(void);
static void torch_off(void);

static struct key_fsm_entry key_fsm_entry[] =
{
	{KEY_IDLE_STATE, KEY_PRESSED_STATE, KEY_PRESS_EVENT, cb_key_1},
	{KEY_PRESSED_STATE, KEY_IDLE_STATE, KEY_RELEASE_EVENT, cb_key_2},
	{KEY_PRESSED_STATE, KEY_TIMEOUT_STATE, KEY_TIMEOUT_EVENT, cb_key_3},
	{KEY_TIMEOUT_STATE, KEY_IDLE_STATE, KEY_RELEASE_EVENT, cb_key_4},
	{KEY_STATE_MAX, 0, 0, NULL},
};

static struct torch_fsm_entry torch_fsm_entry[] =
{
	{TORCH_OFF_STATE, TORCH_ON_STATE, LONG_PRESS_EVENT, cb_torch_1},
	{TORCH_ON_STATE, TORCH_OFF_STATE, LONG_PRESS_EVENT, cb_torch_2},
	{TORCH_ON_STATE, TORCH_OFF_STATE, SHORT_PRESS_EVENT, cb_torch_3},
	{TORCH_ON_STATE, TORCH_OFF_STATE, TORCH_TIMEOUT_EVENT, cb_torch_4},
	{TORCH_ON_STATE, TORCH_DISABLE_STATE, TORCH_DISABLE_EVENT, cb_torch_5},
	{TORCH_OFF_STATE, TORCH_DISABLE_STATE, TORCH_DISABLE_EVENT, cb_torch_6},
	{TORCH_DISABLE_STATE, TORCH_OFF_STATE, TORCH_ENABLE_EVENT, cb_torch_7},
	{TORCH_STATE_MAX, 0, 0, NULL},
};

static void torch_on()
{
	pmic_flash_led_set_current(TORCH_ON_CURRENT);
	huawei_rpc_current_consuem_notify(EVENT_CAMERA_FLASH_NOTIFY, (TORCH_ON_CURRENT/CAMERA_FLASH_CUR_DIV));

	TORCH_DEBUG("torch.c:torch_on\n");
}

static void torch_off()
{
	pmic_flash_led_set_current(TORCH_OFF_CURRENT);
	huawei_rpc_current_consuem_notify(EVENT_CAMERA_FLASH_NOTIFY, 0);

	TORCH_DEBUG("torch.c:torch_off\n");
}

static struct task_struct* torch_fsm_thread_start(void)
{
	struct task_struct *temp = NULL;
	temp = kthread_run(torch_fsm_thread, torch_fsm, "torch_fsm");
	return temp;
}
static void torch_fsm_thread_exit(void)
{
	kthread_stop(torch_fsm->task);
}
static int torch_fsm_thread(void *arg)
{
	struct torch_fsm_data *fsm = (struct torch_fsm_data *) arg;
	struct torch_event_data *event = NULL;

	if(NULL == fsm)
	{
		return -1;
	}

	for(;;)
	{
		wait_event_interruptible(fsm->wait_queue, !list_empty(&fsm->list));

		TORCH_DEBUG("TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT\n");

		if(list_empty(&fsm->list))
		{
			printk("torch.c: TORCH FSM WHY EMPTY!!!!!!!!!!!!!!!!!!!\n");
			schedule();
			continue;
		}

		if (kthread_should_stop())
			break;

		mutex_lock(&fsm->lock);
		while(!list_empty(&fsm->list))
		{
			event = list_entry(fsm->list.next,
					   struct torch_event_data, list);
			list_del(&event->list);
			mutex_unlock(&fsm->lock);

			torch_fsm_call_event(fsm->state, event->event);
			kfree(event);
			mutex_lock(&fsm->lock);
		}
		mutex_unlock(&fsm->lock);
	}

	return 0;
}

static struct torch_event_data* torch_new_event(enum torch_event event)
{
	struct torch_event_data *temp = NULL;

	if(event <= TORCH_EVENT_MIN || event >= TORCH_EVENT_MAX)
	{
		return NULL;
	}

	temp = kzalloc(sizeof(struct torch_event_data), GFP_KERNEL);
	if(NULL == temp)
	{
		return NULL;
	}

	INIT_LIST_HEAD(&temp->list);
	temp->event = event;

	return temp;
}

static int torch_fsm_add_event(struct torch_event_data *event)
{
	if(NULL == event)
	{
		return -1;
	}

	mutex_lock(&torch_fsm->lock);
	list_add_tail(&event->list, &torch_fsm->list);
	mutex_unlock(&torch_fsm->lock);

	wake_up(&torch_fsm->wait_queue);

	return 0;
}

static void torch_timer_init(void)
{
	init_timer(&torch_fsm->torch_timer);
	torch_fsm->torch_timer.expires = jiffies + HZ * LIGHT_TIME_SECS;
	torch_fsm->torch_timer.data = (unsigned long) torch_fsm;
	torch_fsm->torch_timer.function = &torch_timer_func;
}

static void torch_timer_start(void)
{
	add_timer(&torch_fsm->torch_timer);
}

static int  torch_timer_cancel(void)
{
	return del_timer(&torch_fsm->torch_timer);
}

static void torch_timer_func(unsigned long arg)
{
	struct torch_event_data *event = NULL;

	event = torch_new_event(TORCH_TIMEOUT_EVENT);

	if(NULL == event)
	{
		return ;
	}

	if(0 != torch_fsm_add_event(event))
	{
		kfree(event);
		return ;
	}
}

static void cb_torch_1(struct torch_fsm_entry *entry)
{
	if(NULL == torch_fsm)
	{
		return ;
	}

	/* power on flash */
	torch_on();
	
	wake_lock(&torch_wake_lock);

	/* start torch timer */
	torch_timer_init();
	torch_timer_start();

	/* move state */
	torch_fsm->state = entry->final;

}

static void cb_torch_2(struct torch_fsm_entry *entry)
{
	if(NULL == torch_fsm)
	{
		return ;
	}

	/* power off flash */
	torch_off();

	wake_unlock(&torch_wake_lock);

	/* stop torch timer */
	torch_timer_cancel();

	/* move state */
	torch_fsm->state = entry->final;
}
/* TODO: cb_torch_2 == cb_torch_3 */
static void cb_torch_3(struct torch_fsm_entry *entry)
{
	if(NULL == torch_fsm)
	{
		return ;
	}
	/* power off flash */
	torch_off();

	wake_unlock(&torch_wake_lock);

	/* stop torch timer */
	torch_timer_cancel();

	/* move state */
	torch_fsm->state = entry->final;
}

static void cb_torch_4(struct torch_fsm_entry *entry)
{
	if(NULL == torch_fsm)
	{
		return ;
	}

	/* power off flash */
	torch_off();

	wake_unlock(&torch_wake_lock);
	/* move state */
	torch_fsm->state = entry->final;
}

static void cb_torch_5(struct torch_fsm_entry *entry)
{
	if(NULL == torch_fsm)
	{
		return ;
	}

	/* power off flash */
	torch_off();

	wake_unlock(&torch_wake_lock);

	/* stop torch timer */
	torch_timer_cancel();

	/* move state */
	torch_fsm->state = entry->final;
}

static void cb_torch_6(struct torch_fsm_entry *entry)
{
	if(NULL == torch_fsm)
	{
		return ;
	}

	/* move state */
	torch_fsm->state = entry->final;
}

static void cb_torch_7(struct torch_fsm_entry *entry)
{
	if(NULL == torch_fsm)
	{
		return ;
	}

	/* move state */
	torch_fsm->state = entry->final;
}

static void torch_fsm_call_event(enum torch_state c_state, enum torch_event c_event)
{
	struct torch_fsm_entry *action = NULL;

	if(c_state <= TORCH_STATE_MIN || c_state >= TORCH_STATE_MAX)
	{
		return ;
	}

	if(c_event <= TORCH_EVENT_MIN || c_event >= TORCH_EVENT_MAX)
	{
		return ;
	}

	for(action = torch_fsm_entry; action->init != TORCH_STATE_MAX; action++)
	{
		if(action->init == c_state && action->event == c_event)
		{
			break;
		}
	}

	if(action->init == TORCH_STATE_MAX)
	{
		printk("TORCH:unhandled state %d event %d\n", c_state, c_event);
		return ;
	}
	if(action->callb)
	{
		TORCH_DEBUG("TORCH:torch_fsm_call_event state:%d, event:%d!\n", c_state, c_event);
		action->callb(action);
	}
}


static struct task_struct* key_fsm_thread_start(void)
{
	struct task_struct *temp = NULL;
	temp = kthread_run(key_fsm_thread, key_fsm, "key_fsm");
	return temp;
}
static void key_fsm_thread_exit(void)
{
	kthread_stop(key_fsm->task);
}
static int key_fsm_thread(void *arg)
{
	struct key_fsm_data *fsm = (struct key_fsm_data *) arg;
	struct key_event_data *event = NULL;

	if(NULL == fsm)
	{
		return -1;
	}

	for(;;)
	{

		wait_event_interruptible(fsm->wait_queue, !list_empty(&fsm->list));

		TORCH_DEBUG("KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK\n");

		if(list_empty(&fsm->list))
		{
			printk("torch.c: KEY FSM WHY EMPTY!!!!!!!!!!!!!!!!!!!\n");
			schedule();
			continue;
		}

		if (kthread_should_stop())
			break;

		mutex_lock(&fsm->lock);
		while(!list_empty(&fsm->list))
		{
			event = list_entry(fsm->list.next,
					   struct key_event_data, list);
			list_del(&event->list);
			mutex_unlock(&fsm->lock);

			key_fsm_call_event(fsm->state, event->event);
			kfree(event);
			mutex_lock(&fsm->lock);
		}
		mutex_unlock(&fsm->lock);
	}

	return 0;
}

static struct key_event_data* key_new_event(enum key_event event)
{
	struct key_event_data *temp = NULL;

	if(event <= KEY_EVENT_MIN || event >= KEY_EVENT_MAX)
	{
		return NULL;
	}

	temp = kzalloc(sizeof(struct key_event_data ), GFP_KERNEL);
	if(NULL == temp)
	{
		return NULL;
	}

	INIT_LIST_HEAD(&temp->list);
	temp->event = event;

	return temp;
}

static int key_fsm_add_event(struct key_event_data *event)
{
	if(NULL == event)
	{
		return -1;
	}

	mutex_lock(&key_fsm->lock);
	list_add_tail(&event->list, &key_fsm->list);
	mutex_unlock(&key_fsm->lock);

	wake_up(&key_fsm->wait_queue);

	return 0;
}

static void key_timer_init(void)
{
	init_timer(&key_fsm->key_timer);
	/*change the LONG PRESS time,in order to light the torch during sleep state*/
	key_fsm->key_timer.expires = jiffies + HZ * KEY_TIME_SECS/2;
	key_fsm->key_timer.data = (unsigned long) key_fsm;
	key_fsm->key_timer.function = &key_timer_func;
}

static void key_timer_start(void)
{
	add_timer(&key_fsm->key_timer);
}

static int  key_timer_cancel(void)
{
	return del_timer(&key_fsm->key_timer);
}

static void key_timer_func(unsigned long arg)
{
	struct key_event_data *event = NULL;

	event = key_new_event(KEY_TIMEOUT_EVENT);

	if(NULL == event)
	{
		return ;
	}

	if(0 != key_fsm_add_event(event))
	{
		kfree(event);
		return ;
	}
}

static void cb_key_1(struct key_fsm_entry *entry)
{
	if(NULL == key_fsm)
	{
		return ;
	}

	key_fsm->state = entry->final;

	;/* start timer */
	key_timer_init();
	key_timer_start();
}

static void cb_key_2(struct key_fsm_entry *entry)
{
	struct torch_event_data *event = NULL;

	if(NULL == key_fsm)
	{
		return ;
	}

	key_fsm->state = entry->final;

	/* report short press */
	event = torch_new_event(SHORT_PRESS_EVENT);

	/* TODO: logic */
	if(NULL == event)
	{
		return ;
	}

	if(0 != torch_fsm_add_event(event))
	{
		kfree(event);
		return ;
	}
	TORCH_DEBUG("KEY: report SHORT press!\n");

	/* del timer */
	key_timer_cancel();
}

static void cb_key_3(struct key_fsm_entry *entry)
{
	struct torch_event_data *event = NULL;

	if(NULL == key_fsm)
	{
		return ;
	}

	key_fsm->state = entry->final;

	/* report long press */
	event = torch_new_event(LONG_PRESS_EVENT);

	/* TODO: logic */
	if(NULL == event)
	{
		return ;
	}

	if(0 != torch_fsm_add_event(event))
	{
		kfree(event);
		return ;
	}

	TORCH_DEBUG("KEY: report LONG press!\n");
}

static void cb_key_4(struct key_fsm_entry *entry)
{
	if(NULL == key_fsm)
	{
		return ;
	}

	key_fsm->state = entry->final;

}

static void key_fsm_call_event(enum key_state c_state, enum key_event c_event)
{
	struct key_fsm_entry *action = NULL;

	if(c_state <= KEY_STATE_MIN || c_state >= KEY_STATE_MAX)
	{
		return ;
	}

	if(c_event <= KEY_EVENT_MIN || c_event >= KEY_EVENT_MAX)
	{
		return ;
	}

	for(action = key_fsm_entry; action->init != KEY_STATE_MAX; action++)
	{
		if(action->init == c_state && action->event == c_event)
		{
			break;
		}
	}

	if(action->init == KEY_STATE_MAX)
	{
		printk("KEY:unhandled state %d event %d\n", c_state, c_event);
		return ;
	}
	if(action->callb)
	{
		TORCH_DEBUG("KEY:key_fsm_call_event state:%d, event:%d!\n", c_state, c_event);
		action->callb(action);
	}
}


static void torch_event(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{
	struct key_event_data *event=NULL;

	if (type == EV_KEY && code == KEY_TORCH) {
		switch(value){
		case KEY_PRESS:
			/* add KEY_PRESS_EVENT */
			TORCH_DEBUG("KEY_PRESS\n");

			event = key_new_event(KEY_PRESS_EVENT);
			if(NULL == event)
			{
				return ;
			}

			if(0 != key_fsm_add_event(event))
			{
				kfree(event);
				event = NULL;
				return ;
			}

			break;

		case KEY_RELEASE:
			TORCH_DEBUG("KEY_RELEASE\n");

			event = key_new_event(KEY_RELEASE_EVENT);
			if(NULL == event)
			{
				return ;
			}

			if(0 != key_fsm_add_event(event))
			{
				kfree(event);
				event = NULL;
				return ;
			}
			break;

		default:
			break;
		}
	}
	return ;
}

void torch_enable(void)
{
	struct torch_event_data *event = NULL;

	TORCH_DEBUG("torch.c:torch_enable\n");
	if(NULL == key_fsm)
	{
		return ;
	}

	event = torch_new_event(TORCH_ENABLE_EVENT);

	if(NULL == event)
	{
		return ;
	}

	if(0 != torch_fsm_add_event(event))
	{
		kfree(event);
		return ;
	}

	return ;
}

void torch_disable(void)
{
	struct torch_event_data *event = NULL;

	TORCH_DEBUG("torch.c:torch_disable\n");
	if(NULL == key_fsm)
	{
		return ;
	}

	event = torch_new_event(TORCH_DISABLE_EVENT);

	if(NULL == event)
	{
		return ;
	}

	if(0 != torch_fsm_add_event(event))
	{
		kfree(event);
		return ;
	}

	return ;
}

static int torch_connect(struct input_handler *handler, struct input_dev *dev,
			 const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
	{
		printk("torch.c: malloc input_handle error!\n");
		return -ENOMEM;
	}

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "torch";

	error = input_register_handle(handle);
	if (error)
	{
		printk("torch.c input_register_handle error!\n");
		goto err_free_handle;
	}

	error = input_open_device(handle);
	if (error)
	{
		printk("torch.c input_open_device error!\n");
		goto err_unregister_handle;
	}

	TORCH_DEBUG("torch.c: Connected device: %s (%s at %s)\n",
				dev_name(&dev->dev),
				dev->name ?: "unknown",
				dev->phys ?: "unknown");

	return 0;

 err_unregister_handle:
	input_unregister_handle(handle);
 err_free_handle:
	kfree(handle);
	return error;
}

static void torch_disconnect(struct input_handle *handle)
{
	TORCH_DEBUG("torch.c: Disconnected device: %s\n",
				dev_name(&handle->dev->dev));

	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id torch_ids[] = {
	{ .driver_info = 1 },	/* Matches all devices */
	{ },			/* Terminating zero entry */
};

MODULE_DEVICE_TABLE(input, torch_ids);

static struct input_handler torch_handler = {
	.event 		=	torch_event,
	.connect 	=	torch_connect,
	.disconnect 	=	torch_disconnect,
	.name 		=	"torch",
	.id_table 	=	torch_ids,
};

static int __init torch_init(void)
{	
	int error;

	if(machine_is_msm7x25_u8300())
	{
		torch_fsm = kzalloc(sizeof(struct torch_fsm_data), GFP_KERNEL);
		if(NULL == torch_fsm)
		{
			printk("torch.c: malloc torch_fsm_data error!\n");
			return -ENOMEM;
		}

		wake_lock_init(&torch_wake_lock, WAKE_LOCK_SUSPEND, "torch");
		mutex_init(&torch_fsm->lock);
		INIT_LIST_HEAD(&torch_fsm->list);
		torch_fsm->state = TORCH_OFF_STATE;
		init_waitqueue_head(&torch_fsm->wait_queue);

		key_fsm = kzalloc(sizeof(struct key_fsm_data), GFP_KERNEL);
		if(NULL == key_fsm)
		{
			kfree(torch_fsm);
			printk("torch.c: malloc key_fsm_data error!\n");
			return -ENOMEM;
		}

		mutex_init(&key_fsm->lock);
		INIT_LIST_HEAD(&key_fsm->list);
		key_fsm->state = KEY_IDLE_STATE;
		init_waitqueue_head(&key_fsm->wait_queue);

		key_fsm->task = key_fsm_thread_start();
		if(IS_ERR(key_fsm->task))
		{
			printk("torch.c: create key_fsm_start error!\n");
			kfree(torch_fsm);
			kfree(key_fsm);
			return -ENOMEM;
		}

		torch_fsm->task = torch_fsm_thread_start();
		if(IS_ERR(torch_fsm->task))
		{
			printk("torch.c: torch_fsm_start error!\n");
			key_fsm_thread_exit();
			kfree(torch_fsm);
			kfree(key_fsm);
			return -ENOMEM;
		}

		return input_register_handler(&torch_handler);
	}
	else
	{
		return 0;
	}
}

static void __exit torch_exit(void)
{
	if(machine_is_msm7x25_u8300())
	{
		torch_fsm_thread_exit();
		key_fsm_thread_exit();

		kfree(torch_fsm);
		kfree(key_fsm);
		wake_lock_destroy(&torch_wake_lock);

		input_unregister_handler(&torch_handler);
	}
}

module_init(torch_init);
module_exit(torch_exit);

MODULE_AUTHOR("Huawei");
MODULE_DESCRIPTION("Torch");
MODULE_LICENSE("GPL");
