/*
 * drivers/serial/msm_serial.c - driver for msm7k serial device and console
 *
 * Copyright (C) 2007 Google, Inc.
 * Author: Robert Love <rlove@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

//#define SHMEM_TTY_DEBUG

#if defined(CONFIG_SHMEM_SERIAL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
# define SUPPORT_SYSRQ
#endif

#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <asm-arm/huawei/andmsm_share.h>

#define SHMEM_TTY_DEBUG

static int  port_initialized = 0;
static struct semaphore shmem_serial_open_close_sem[ANDMSM_SIO_PORT_MAX];
struct shmem_serial_port {
	struct tty_struct 	*port_tty;	/* pointer to tty struct */
	int			port_num;
	spinlock_t 		port_lock;
	int 			port_open_count;
}ss_port[ANDMSM_SIO_PORT_MAX];


static const struct {
	andmsm_port_num_enum		minor;
	char			*name;
} cdevlist[ANDMSM_SIO_PORT_MAX] = { /* list of minor devices */
  {ANDMSM_SIO_PORT_SRV_CHN_0,       "ttySrvChn0"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_1,       "ttySrvChn1"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_2,       "ttySrvChn2"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_3,       "ttySrvChn3"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_4,       "ttySrvChn4"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_5,       "ttySrvChn5"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_6,       "ttySrvChn6"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_7,       "ttySrvChn7"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_8,       "ttySrvChn8"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_9,       "ttySrvChn9"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_A,       "ttySrvChnA"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_B,       "ttySrvChnB"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_C,       "ttySrvChnC"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_D,       "ttySrvChnD"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_E,       "ttySrvChnE"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_F,       "ttySrvChnF"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_10,       "ttySrvChn10"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_11,       "ttySrvChn11"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_12,       "ttySrvChn12"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_13,       "ttySrvChn13"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_14,       "ttySrvChn14"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_15,       "ttySrvChn15"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_16,       "ttySrvChn16"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_17,       "ttySrvChn17"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_18,       "ttySrvChn18"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_19,       "ttySrvChn19"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_1A,       "ttySrvChn1A"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_1B,       "ttySrvChn1B"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_1C,       "ttySrvChn1C"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_1D,       "ttySrvChn1D"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_1E,       "ttySrvChn1E"}
  ,{ANDMSM_SIO_PORT_SRV_CHN_1F,       "ttySrvChn1F"}
};


/* tty driver struct */
static int shmem_serial_open(struct tty_struct *tty, struct file *file);
static void shmem_serial_close(struct tty_struct *tty, struct file *file);
static int shmem_serial_write(struct tty_struct *tty, const unsigned char *buf, int count);
static int shmem_serial_ioctl(struct tty_struct *tty, struct file *file, unsigned int cmd, unsigned long arg);
static int shmem_serial_write_room(struct tty_struct *tty);
int shmem_serial_recv_data(int port_num, char *data, unsigned int size);
static int __init shmem_console_init(void);

static const struct tty_operations shmem_tty_ops = {
	.open = shmem_serial_open,
	.close = shmem_serial_close,
	.write = shmem_serial_write,
	.ioctl = shmem_serial_ioctl,
	.write_room = shmem_serial_write_room,
/*
	.put_char = shmem_serial_put_char,
	.flush_chars = shmem_serial_flush_chars,
	.set_termios = shmem_serial_set_termios,
	.throttle = shmem_serial_throttle,
	.unthrottle = shmem_serial_unthrottle,
	.break_ctl = shmem_serial_break,
	.chars_in_buffer = shmem_serial_chars_in_buffer,
	.tiocmget = shmem_serial_tiocmget,
	.tiocmset = shmem_serial_tiocmset,
*/
};

static struct tty_driver *shmem_tty_driver;

extern void virtmsm_msg_run(void);
extern void virtmsm_msg_stop(void);
static int shmem_serial_open_cnt = 0;
/* TTY Driver */
/*
 * shmem_serial_open
 */
static int shmem_serial_open(struct tty_struct *tty, struct file *file)
{
	int 			port_num;
	unsigned long 		flags;
	struct semaphore 	*sem;
	int 			ret;
	struct shmem_serial_port *port;

	port_num = tty->index;

#ifdef SHMEM_TTY_DEBUG	
	printk("shmem_serial_open: (%d,%p,%p)\n", port_num, tty, file);
#endif

	if (port_num < 0 || port_num >= ANDMSM_SIO_PORT_MAX) 
	{
		printk(KERN_ERR "shmem_serial_open: (%d,%p,%p) invalid port number\n",
		       port_num, tty, file);
		return -ENODEV;
	}
	
	port = &ss_port[port_num];
	
	sem = &shmem_serial_open_close_sem[port_num];

	if (down_interruptible(sem)) 
	{
		printk(KERN_ERR"shmem_serial_open: (%d,%p,%p) interrupted waiting for semaphore\n", port_num, tty, file);
		return -ERESTARTSYS;
	}
	spin_lock_irqsave(&port->port_lock, flags);
	
	port->port_tty = tty;
	port->port_open_count++;
	tty->driver_data = port;
	
#ifdef SHMEM_TTY_DEBUG	
	printk("shmem_serial_open: (%d,%p,%p) completed\n", port_num, tty, file);
#endif
	ret = 0;

	spin_unlock_irqrestore(&port->port_lock, flags);
	up(sem);
	shmem_serial_open_cnt++;
	if (1 == shmem_serial_open_cnt)
	{
		virtmsm_msg_run();
	}
	return ret;
}

/*
 * shmem_serial_close
 */
static void shmem_serial_close(struct tty_struct *tty, struct file *file)
{
	struct shmem_serial_port *port = tty->driver_data;
	struct semaphore 	*sem;
	unsigned long 		flags;

	if (port == NULL) 
	{
		printk(KERN_ERR "shmem_serial_close: NULL port pointer\n");
		return;
	}
#ifdef SHMEM_TTY_DEBUG
	printk("shmem_serial_close: (%p,%p)\n", tty, file);
#endif
	sem = &shmem_serial_open_close_sem[port->port_num];
	if (down_interruptible(sem)) 
	{
		printk(KERN_ERR"shmem_serial_close: (%p,%p) interrupted waiting for semaphore\n", tty, file);
		return;
	}
	spin_lock_irqsave(&port->port_lock, flags);
	
	//port->port_tty = NULL;
	//port->port_num = 0;
	port->port_open_count--;

#ifdef SHMEM_TTY_DEBUG
	printk("shmem_serial_close: (%p,%p) completed\n", tty, file);
#endif
	spin_unlock_irqrestore(&port->port_lock, flags);
	up(sem);
	if (shmem_serial_open_cnt > 0) {
		shmem_serial_open_cnt--;
	} else {
		shmem_serial_open_cnt = 0;
	}
	if (0 == shmem_serial_open_cnt)
	{
		virtmsm_msg_stop();
	}
	return;
}

extern uint32_t virtmsm_sio_write(int port_num, void* pData, uint32_t length);
/*
 * shmem_serial_write
 */
static int shmem_serial_write(struct tty_struct *tty, const unsigned char *buf, int count)
{
	unsigned long 			flags;
	int 					ret;
#ifdef SHMEM_TTY_DEBUG
	char					ss[128];
	int					    len;
#endif
	
	struct shmem_serial_port 	*port= tty->driver_data;

	if (port == NULL) 
	{
		printk(KERN_ERR "shmem_serial_write: error port pointer\n");
		return -EIO;
	}

#ifdef SHMEM_TTY_DEBUG
	printk("shmem_serial_write: (%d,%p) writing %d bytes\n", port->port_num, tty, count);
	len = (count>127) ? 127 : count;
	memset(ss, 0, 128);
	memcpy(ss, buf, len);

	printk("Content start -----------------\n");
	printk("Content: %s\n", ss);
	printk("Content over -----------------\n");
#endif	

	
	spin_lock_irqsave(&port->port_lock, flags);

	if (port->port_open_count == 0) 
	{
		printk(KERN_ERR "shmem_serial_write: (%d,%p) port is closed\n",
		       port->port_num, tty);
		ret = -EBADF;
		goto exit;
	}

	//#################################
	//ret = ss_buf_put(buf, count);
	ret =  virtmsm_sio_write(port->port_num, (void*)buf, count);

exit:
	spin_unlock_irqrestore(&port->port_lock, flags);
	return ret;

}

/*
 * shmem_serial_ioctl
 */
static int shmem_serial_ioctl(struct tty_struct *tty, struct file *file,
		    unsigned int cmd, unsigned long arg)
{
	struct shmem_serial_port *port = tty->driver_data;
	DECLARE_WAITQUEUE(wait, current);
	struct async_icount cnow;
	struct async_icount cprev;

    cnow = cnow;
    cprev = cprev;
    wait = wait;

	if (port == NULL) {
		printk(KERN_ERR "shmem_serial_ioctl: NULL port pointer\n");
		return -EIO;
	}
#ifdef SHMEM_TTY_DEBUG
	printk("shmem_serial_ioctl: (%d,%p,%p) cmd=0x%4.4x, arg=%lu\n",
		 port->port_num, tty, file, cmd, arg);
#endif

	/* handle ioctls */
	switch (cmd) {
	case TIOCMIWAIT:
/*
		cprev = shmem_serial_current_icount(port);
		while (1) {
			add_wait_queue(&port->msr_change_wait, &wait);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			remove_wait_queue(&port->msr_change_wait, &wait);
			if (signal_pending(current))
				return -ERESTARTSYS;
			cnow = shmem_serial_current_icount(port);
			if (cnow.rng == cprev.rng &&
				cnow.dsr == cprev.dsr &&
				cnow.dcd == cprev.dcd &&
				cnow.cts == cprev.cts)
				return -EIO;
			if (((arg & TIOCM_RI) &&
					(cnow.rng != cprev.rng)) ||
				((arg & TIOCM_DSR) &&
					(cnow.dsr != cprev.dsr)) ||
				((arg & TIOCM_CD)  &&
					(cnow.dcd != cprev.dcd)) ||
				((arg & TIOCM_CTS) &&
					(cnow.cts != cprev.cts)))
				return 0;
			cprev = cnow;
		}
*/
		break;
	}

	/* could not handle ioctl */
	return -ENOIOCTLCMD;
}

static int shmem_serial_recv( struct shmem_serial_port *port, char *packet, unsigned int size)
{
	unsigned int 		len;
	int 				ret;
	struct tty_struct 	*tty;

	if (port == NULL) 
	{
		printk(KERN_ERR "shmem_serial_recv: NULL port pointer\n");
		return -EIO;
	}

#ifdef SHMEM_TTY_DEBUG
	printk("shmem_serial_recv: (%d, %p, %d) \n", port->port_num, packet, size);
#endif

	spin_lock(&port->port_lock);
	//printk("#####We got spin lock####\n");
	if (port->port_open_count == 0) 
	{
		//printk("shmem_serial_recv: port=%d, port is closed\n", port->port_num);
		ret = -EIO;
		goto exit;
	}

	tty = port->port_tty;

	if (tty == NULL) 
	{
		printk(KERN_ERR "shmem_serial_recv: port=%d, NULL tty pointer\n", port->port_num);
		ret = -EIO;
		goto exit;
	}

	if (port->port_tty->magic != TTY_MAGIC) 
	{
		printk(KERN_ERR "shmem_serial_recv: port=%d, bad tty magic\n", port->port_num);
		ret = -EIO;
		goto exit;
	}
	
	len = tty_buffer_request_room(tty, size);

	if (len > 0) 
	{
	    int nRet = tty_insert_flip_string(tty, packet, size);
	
	    if (size != nRet)
	    {
	        printk(KERN_ERR "tty_insert_flip_string failed! nRet=%d\n", nRet);
	    }
		
		tty_flip_buffer_push(port->port_tty);
		wake_up_interruptible(&port->port_tty->read_wait);
        printk(KERN_ERR "shmem_serial_recv: success! len=%d\n", len);
        ret = 0;
	}
    else
    {
        printk(KERN_ERR "shmem_serial_recv: no memory!\n");
        ret = -ENOMEM;
    }
	
exit:
	spin_unlock(&port->port_lock);
	return ret;
}
/*
* shmem_serial_recv_data
*/
int shmem_serial_recv_data(int port_num, char *data, unsigned int size)
{
	if(ss_port[port_num].port_open_count == 0)
	{
	  printk("The port %d is not opened!\n", port_num);
	  return 0;
	}
	else
    {
	  return shmem_serial_recv(&ss_port[port_num], data, size);
	}
}

/*
 * shmem_serial_write_room
 */
static int shmem_serial_write_room(struct tty_struct *tty)
{

	int room = 0;
	unsigned long flags;
	struct shmem_serial_port *port = tty->driver_data;

	if (port == NULL)
		return 0;

	spin_lock_irqsave(&port->port_lock, flags);
/*
	if (port->port_dev != NULL && port->port_open_count > 0
	    && port->port_write_buf != NULL)
		room = shmem_serial_buf_space_avail(port->port_write_buf);
*/
	room = 256;

	spin_unlock_irqrestore(&port->port_lock, flags);

	//printk("shmem_serial_write_room: (%d,%p) room=%d\n", port->port_num, tty, room);

	return room;
}


#if 0
/*
 * shmem_serial_put_char
 */
static void shmem_serial_char(struct tty_struct *tty, unsigned char ch)
{
	unsigned long 		flags;
	struct shmem_serial_port *port = tty->driver_data;

	if (port == NULL) 
	{
		printk(KERN_ERR "shmem_serial_put_char: NULL port pointer\n");
		return;
	}

	printk("shmen_serial_put_char: (%d,%p) char=0x%x, called from %p, %p, %p\n",
		 port->port_num, tty, ch, __builtin_return_address(0),
		 __builtin_return_address(1), __builtin_return_address(2));

	spin_lock_irqsave(&port->port_lock, flags);

	if (port->port_open_count == 0) 
	{
		printk(KERN_ERR "shmem_serial_put_char: (%d,%p) port is closed\n",
		       port->port_num, tty);
		goto exit;
	}
	shmen_serial_buf_put(port->port_write_buf, &ch, 1);
		
exit:
	spin_unlock_irqrestore(&port->port_lock, flags);
}

/*
 * shmem_serial_flush_chars
 */
static void shmem_serial_flush_chars(struct tty_struct *tty)
{
	unsigned long flags;
	struct shmem_serial_port *port = tty->driver_data;

	if (port == NULL) {
		printk(KERN_ERR "shmem_serial_flush_chars: NULL port pointer\n");
		return;
	}

	printk("shmem_serial_flush_chars: (%d,%p)\n", port->port_num, tty);

	spin_lock_irqsave(&port->port_lock, flags);

	if (port->port_dev == NULL) {
		printk(KERN_ERR
		       "shmem_serial_flush_chars: (%d,%p) port is not connected\n",
		       port->port_num, tty);
		goto exit;
	}

	if (port->port_open_count == 0) {
		printk(KERN_ERR "shmem_serial_flush_chars: (%d,%p) port is closed\n",
		       port->port_num, tty);
		goto exit;
	}

	spin_unlock_irqrestore(&port->port_lock, flags);

	//shmem_serial_send(shmem_serial_devices[tty->index]);

	return;

exit:
	spin_unlock_irqrestore(&port->port_lock, flags);
}


/*
 * shmem_serial_chars_in_buffer
 */
static int shmem_serial_chars_in_buffer(struct tty_struct *tty)
{
	int chars = 0;
	unsigned long flags;
	struct shmem_serial_port *port = tty->driver_data;

	if (port == NULL)
		return 0;

	spin_lock_irqsave(&port->port_lock, flags);

	if (port->port_dev != NULL && port->port_open_count > 0
	    && port->port_write_buf != NULL)
		chars = shmem_serial_buf_data_avail(port->port_write_buf);

	spin_unlock_irqrestore(&port->port_lock, flags);

	printk("shmem_serial_chars_in_buffer: (%d,%p) chars=%d\n",
		 port->port_num, tty, chars);

	return chars;
}

/*
 * shmem_serial_throttle
 */
static void shmem_serial_throttle(struct tty_struct *tty)
{
}

/*
 * shmem_serial_unthrottle
 */
static void shmem_serial_unthrottle(struct tty_struct *tty)
{
}

/*
 * shmem_serial_break
 */
static void shmem_serial_break(struct tty_struct *tty, int break_state)
{
}
/*
 * shmem_serial_set_termios
 */
static void shmem_serial_set_termios(struct tty_struct *tty, struct ktermios *old)
{
}
#endif


/*
* shmem_serial_init
*
* Register as a tty driver .
*/
extern struct device *tty_register_device_virtname(struct tty_driver *driver,
						  unsigned index, struct device *dev, char *tty_name);

static int __init shmem_serial_init(void)
{
	int i, retval;


	printk("Shared memory serial init\n");
	shmem_tty_driver = alloc_tty_driver(ANDMSM_SIO_PORT_MAX);
	if (!shmem_tty_driver)
		return -ENOMEM;
		
	shmem_tty_driver->owner 	= THIS_MODULE;
	shmem_tty_driver->driver_name 	= "shmem_tty";
	shmem_tty_driver->name 		= "ttySMEM"; 
	shmem_tty_driver->major 	= 129;
	shmem_tty_driver->minor_start 	= 0;
	shmem_tty_driver->type 		= TTY_DRIVER_TYPE_SERIAL;
	shmem_tty_driver->subtype 	= SERIAL_TYPE_NORMAL;
	shmem_tty_driver->flags 	= TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV | TTY_DRIVER_RESET_TERMIOS;
	shmem_tty_driver->init_termios 	= tty_std_termios;
	shmem_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	
	tty_set_operations(shmem_tty_driver, &shmem_tty_ops);

	for (i = 0; i < ANDMSM_SIO_PORT_MAX; i++)
		sema_init(&shmem_serial_open_close_sem[i], 1);

	if(!port_initialized)
	{
		for (i = 0; i < ANDMSM_SIO_PORT_MAX; i++)	
		{
			memset(&ss_port[i], 0, sizeof(struct shmem_serial_port));	
			spin_lock_init(&ss_port[i].port_lock);
			ss_port[i].port_num = i;
		}
		port_initialized = 1;
	}

	retval = tty_register_driver(shmem_tty_driver);
	
	if (retval) 
	{
		put_tty_driver(shmem_tty_driver);
		printk(KERN_ERR "shmem_serial_init: cannot register tty driver,ret = %d\n", retval);
		return retval;
	}
	
	for (i = 0; i < ANDMSM_SIO_PORT_MAX; i++)
        tty_register_device_virtname(shmem_tty_driver, i, NULL, cdevlist[i].name);

	shmem_console_init();

	return 0;
}

/*
* shmem_serial_exit
*
* Unregister as a tty driver .
*/
static void __exit shmem_serial_exit(void)
{
	int i;

	for (i = 0; i < ANDMSM_SIO_PORT_MAX; i++)
		tty_unregister_device(shmem_tty_driver, i);
		
	tty_unregister_driver(shmem_tty_driver);
	put_tty_driver(shmem_tty_driver);
	printk(KERN_INFO "shmem_serial_module_exit\n" );
}

module_init(shmem_serial_init);
module_exit(shmem_serial_exit);

MODULE_AUTHOR("Jim SUN");
MODULE_DESCRIPTION("Driver for shmem tty device");
MODULE_LICENSE("GPL");

//-------------------------------------------------------
//console
//-------------------------------------------------------

/* Console write */
static void smem_console_write(struct console *co, const char *s, u_int count)
{
	struct shmem_serial_port *port;
	unsigned long 		flags;
	char				tmp[2] = {0xd,0x0};
	u_int			start, end;
	
	port = (struct shmem_serial_port *)co->data;
	if (port == NULL) 
	{
		return;
	}
	
	spin_lock_irqsave(&port->port_lock, flags);

	//#################################
	//ret = ss_buf_put(s, count);
	//#################################
	start = end =0;
	while(end < count)
	{	
		if(*(s + end) == 0xA)
		{
			if(end > start)
			{
				virtmsm_sio_write(port->port_num, (void*)(s + start), end - start);
			}
			virtmsm_sio_write(port->port_num, (void*)tmp, 1);
			start = end;
			end ++;
		}
		else
			end ++;		
	}
	if(start < end)
	{
			virtmsm_sio_write(port->port_num, (void*)(s + start), end - start);
	}
	spin_unlock_irqrestore(&port->port_lock, flags); 
	//the following printk should not be opened unless u want to test if the console is ok
	//when opened, "****smem_console_write call****" will be printed over and over times
	//printk("****smem_console_write call****\n");
	

	return;

}

/* Setup console communication parameters */
static int __init smem_console_setup(struct console *co, char *options)
{
	int	ret = 0, i;
	struct	shmem_serial_port *port;
	
	//printk("Shared memory console setup...");
	if( !port_initialized)
	{
		for (i = 0; i < ANDMSM_SIO_PORT_MAX; i++)	
		{
			memset(&ss_port[i], 0, sizeof(struct shmem_serial_port));	
			spin_lock_init(&ss_port[i].port_lock);
		}	
		port_initialized = 1;	
	}
	
	if (co->index == -1 || co->index >= ANDMSM_SIO_PORT_MAX) 
	{
    		co->index = 0;
  	}
  	port = &ss_port[co->index];  	
	co->data = port;
	printk("Done\n");
	return ret;
}


static struct console shmem_console = {
  .name    = "ttySMEM",                 /* Console name */
  .write   = smem_console_write,  	/* How to printk to the console */
  .device  = uart_console_device,     	/* Provided by the serial core */
  .setup   = smem_console_setup,  	/* How to setup the console */
  .flags   = CON_PRINTBUFFER,         	/* Default flag */
  .index   = -1,                      	/* Init to invalid value */
};

/* Console Initialization */
static int __init shmem_console_init(void)
{
  register_console(&shmem_console);
  return 0;
}
//console_initcall(shmem_console_init);

