/*
 *Desc: a kenel crash dump driver, dump the key message to a reserved memory 
 *     region when system crashes
 *Auther: yKF14048
 *Date: 2009-06-23
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/kdebug.h>
#include <linux/notifier.h>
#include <linux/kallsyms.h>
#include <linux/thread_info.h>
#include <linux/utsname.h>
#include <asm/processor.h> 
#include <asm/system.h>    
#include <linux/delay.h>
#include <linux/hardirq.h>

#include "../../arch/arm/mach-msm/smd_private.h"
#include "../../arch/arm/mach-msm/smd_rpcrouter.h"
#include "../../arch/arm/mach-msm/proc_comm.h"

struct mem_crashdump_dev {
    const char *name;
    void *dump_base; 
    unsigned long dump_size;
    loff_t dump_off; 
    struct device *pdevice; 
    struct cdev cdev; 
    struct semaphore sem; 
    struct semaphore open_sem; 
} ;

static dev_t crashdump_devno; 
static struct mem_crashdump_dev *crashdump_dev = NULL; 
static int crashdump_debug = 1; 
static struct class *crashdump_class = NULL; 
static char *crashdump_name = "crashdump"; 

//static unsigned long crashdump_base = 0;
static unsigned long crashdump_size = 0; 
static unsigned long crashdump_magic = 0xdeadbeaf; 
static unsigned long crashdump_header = 32; 
static unsigned long crashdump_tailer = 4; 
static int crashdump_depth = 10; 

#define crashdump_printf(fmt, args...) \
    if (crashdump_debug) \
        printk(KERN_ERR fmt, ##args); \
    else { \
    } 

static int crashdump_open(struct inode *pnode, struct file *filep)
{
    int i = 0; 
    crashdump_printf("%s\n", __FUNCTION__); 
   
    if (down_interruptible(&crashdump_dev->open_sem))
    {
        return -ERESTARTSYS; 
    }

    /* judge if the magic code is correct */
    if ( (*(unsigned long *)(crashdump_dev->dump_base)) != crashdump_magic)
    {
        crashdump_printf("crashdump magic is not correct\n"); 
	goto failed;
    }

    /* get the size of dump */
    crashdump_dev->dump_size = *(unsigned long *)(crashdump_dev->dump_base + 4)
        - crashdump_header - crashdump_tailer; 
    if (crashdump_dev->dump_size > crashdump_size)
    {
        crashdump_printf("crashdump size is too large\n"); 
	goto failed; 
    }

    /* judget if the size series is correct */
    for (i = 2; i < 7; i++)
    {
        if ( (*(unsigned long *)(crashdump_dev->dump_base + i * 4))
            != (crashdump_dev->dump_size + i - 1))
	{
	    crashdump_printf("crashdump header is not correct %d\n", i); 
	    goto failed; 
	}
    }
    
    crashdump_dev->dump_off = 0;
    filep->private_data = crashdump_dev; 
    init_MUTEX(&crashdump_dev->sem); 

    return 0; 

failed: 
    iounmap(crashdump_dev->dump_base); 
    crashdump_dev->dump_base = 0; 
    crashdump_dev->dump_size = 0; 
    crashdump_printf("%s exit\n", __FUNCTION__); 
    up(&crashdump_dev->open_sem); 
    return -EFAULT;
}

static int crashdump_release(struct inode *pnode, struct file *filp)
{
    struct mem_crashdump_dev *dev = (struct mem_crashdump_dev *)filp->private_data;

    crashdump_printf("%s\n", __FUNCTION__);

    if (dev->dump_base)
    {
        iounmap(dev->dump_base); 
        dev->dump_base = 0;
        dev->dump_size = 0; 
    }

    up(&dev->open_sem);

    return 0; 
}

static ssize_t crashdump_read(struct file *filp, char __user *buff, 
              size_t count, loff_t *offp)
{
    struct mem_crashdump_dev *dev = (struct mem_crashdump_dev *)filp->private_data; 
    ssize_t ret = 0; 

    crashdump_printf("%s\n", __FUNCTION__); 
    
    if (down_interruptible(&dev->sem))
    {
        return -ERESTARTSYS; 
    }
    if (*offp > dev->dump_size)
    {
        goto out; 
    }

    if (*offp + count > dev->dump_size)
    {
        count = dev->dump_size - *offp; 
    }

    if (copy_to_user(buff, dev->dump_base + crashdump_header, count)) 
    {
        ret = -EFAULT; 
	goto out; 
    }

    *offp += count; 
    ret = count; 

out:
    up(&dev->sem); 
    return ret; 	      
}

static ssize_t crashdump_write(struct file *filp, const char __user *buff, 
             size_t count, loff_t *offp) 
{
    crashdump_printf("%s\n", __FUNCTION__); 
    return 0; 
}

static struct file_operations crashdump_fops = 
{
    .owner = THIS_MODULE, 
    .open = crashdump_open, 
    .read = crashdump_read,
    .write = crashdump_write,
    .release = crashdump_release
};

static int crashdump_modules(void *buf) 
{
   return sprintf_modules(buf); 
}

static const char *processor_modes[] = {
	"USER_26", "FIQ_26" , "IRQ_26" , "SVC_26" , "UK4_26" , "UK5_26" , "UK6_26" , "UK7_26" ,
	"UK8_26" , "UK9_26" , "UK10_26", "UK11_26", "UK12_26", "UK13_26", "UK14_26", "UK15_26",
	"USER_32", "FIQ_32" , "IRQ_32" , "SVC_32" , "UK4_32" , "UK5_32" , "UK6_32" , "ABT_32" ,
	"UK8_32" , "UK9_32" , "UK10_32", "UND_32" , "UK12_32", "UK13_32", "UK14_32", "SYS_32"
};

static const char *isa_modes[] = {
	"ARM" , "Thumb" , "Jazelle", "ThumbEE"
};
static int crashdump_registers(void *buf, struct pt_regs *regs) 
{
	unsigned long flags;
	char local_buf[64];
	char symbol_buf[KSYM_SYMBOL_LEN] = { 0 };
        int len = 0, tmp_len = 0; 
	tmp_len = sprintf(buf, "CPU: %d    %s  (%s %.*s)\n",
			smp_processor_id(), print_tainted(), init_utsname()->release,
			(int)strcspn(init_utsname()->version, " "),
			init_utsname()->version);
	buf += tmp_len; 
	len += tmp_len;

	//print_symbol("PC is at %s\n", instruction_pointer(regs));
	sprint_symbol(symbol_buf, instruction_pointer(regs)); 
        tmp_len = sprintf(buf, "PC is at %s\n", symbol_buf); 
	buf += tmp_len; 
	len += tmp_len; 
	//print_symbol("LR is at %s\n", regs->ARM_lr);
	sprint_symbol(symbol_buf, regs->ARM_lr); 
        tmp_len = sprintf(buf, "LR is at %s\n", symbol_buf); 
	buf += tmp_len; 
	len += tmp_len; 
	
	tmp_len = sprintf(buf, "pc : [<%08lx>]    lr : [<%08lx>]    psr: %08lx\n"
			"sp : %08lx  ip : %08lx  fp : %08lx\n",
			regs->ARM_pc, regs->ARM_lr, regs->ARM_cpsr,
			regs->ARM_sp, regs->ARM_ip, regs->ARM_fp);
	buf += tmp_len; 
	len += tmp_len; 

	tmp_len = sprintf(buf, "r10: %08lx  r9 : %08lx  r8 : %08lx\n",
			regs->ARM_r10, regs->ARM_r9,
			regs->ARM_r8);
	buf += tmp_len; 
	len += tmp_len; 

	tmp_len = sprintf(buf, "r7 : %08lx  r6 : %08lx  r5 : %08lx  r4 : %08lx\n",
			regs->ARM_r7, regs->ARM_r6,
			regs->ARM_r5, regs->ARM_r4);
	buf += tmp_len; 
	len += tmp_len; 

	tmp_len = sprintf(buf, "r3 : %08lx  r2 : %08lx  r1 : %08lx  r0 : %08lx\n",
			regs->ARM_r3, regs->ARM_r2,
			regs->ARM_r1, regs->ARM_r0);
	buf += tmp_len; 
	len += tmp_len; 


	flags = regs->ARM_cpsr;
	local_buf[0] = flags & PSR_N_BIT ? 'N' : 'n';
	local_buf[1] = flags & PSR_Z_BIT ? 'Z' : 'z';
	local_buf[2] = flags & PSR_C_BIT ? 'C' : 'c';
	local_buf[3] = flags & PSR_V_BIT ? 'V' : 'v';
	local_buf[4] = '\0';

	tmp_len = sprintf(buf, "Flags: %s  IRQs o%s  FIQs o%s  Mode %s  ISA %s  Segment %s\n",
			local_buf, interrupts_enabled(regs) ? "n" : "ff",
			fast_interrupts_enabled(regs) ? "n" : "ff",
			processor_modes[processor_mode(regs)],
			isa_modes[isa_mode(regs)],
			get_fs() == get_ds() ? "kernel" : "user");
	len += tmp_len; 
        buf += tmp_len; 

#ifdef CONFIG_CPU_CP15
	{
		unsigned int ctrl;

		local_buf[0] = '\0';
#ifdef CONFIG_CPU_CP15_MMU
		{
			unsigned int transbase, dac;
			asm("mrc p15, 0, %0, c2, c0\n\t"
					"mrc p15, 0, %1, c3, c0\n"
					: "=r" (transbase), "=r" (dac));
			snprintf(local_buf, sizeof(local_buf), "  Table: %08x  DAC: %08x",
					transbase, dac);
		}
#endif
		asm("mrc p15, 0, %0, c1, c0\n" : "=r" (ctrl));

		tmp_len = sprintf(buf, "Control: %08x%s\n", ctrl, local_buf);
		buf += tmp_len; 
		len += tmp_len; 
	}
#endif
	return len; 
}


static int crashdump_stack(void *buf, struct pt_regs *regs) 
{
	struct thread_info *thread = current_thread_info(); 
	struct task_struct *tsk = thread->task; 
        unsigned long bottom = regs->ARM_sp; 
	unsigned long top = THREAD_SIZE + (unsigned long)task_stack_page(tsk); 
	unsigned long p = bottom & ~31;
	mm_segment_t fs;
	int i, tmp_len = 0, len = 0;

        crashdump_printf("%s\n", __FUNCTION__); 

	/*
	 * We need to switch to kernel mode so that we can use __get_user
	 * to safely read from kernel space.  Note that we now dump the
	 * code first, just in case the backtrace kills us.
	 */
	fs = get_fs();
	set_fs(KERNEL_DS);

	tmp_len = sprintf(buf, "%s(0x%08lx to 0x%08lx)\n", "Stack: ", bottom, top);
        len += tmp_len; 
	buf += tmp_len; 

	for (p = bottom & ~31; p < top;) {
		tmp_len = sprintf(buf, "%04lx: ", p & 0xffff);
		len += tmp_len; 
		buf += tmp_len; 
		for (i = 0; i < 8; i++, p += 4) {
			unsigned int val;

			if (p < bottom || p >= top)
			{
				tmp_len = sprintf(buf, "         ");
				len += tmp_len; 
				buf += tmp_len; 
			}
			else {
				__get_user(val, (unsigned long *)p);
				tmp_len = sprintf(buf, "%08x ", val);
				len += tmp_len; 
				buf += tmp_len; 
			}
		}
		tmp_len = sprintf(buf, "\n");
		len += tmp_len; 
		buf += tmp_len; 
	}

	set_fs(fs);

	return len; 
}
struct stackframe {
	unsigned long fp;
	unsigned long sp;
	unsigned long lr;
	unsigned long pc;
};


static int walk_stackframe(unsigned long fp, unsigned long low, unsigned long high,
		int (*fn)(struct stackframe *, void *), int depth,  void *buf)
{
	struct stackframe *frame;
        int tmp_len=0, len = 0;
	do {
		/*
		 * Check current frame pointer is within bounds
		 */
		if (fp < (low + 12) || fp + 4 >= high)
			break;

		frame = (struct stackframe *)(fp - 12);

		tmp_len = fn(frame, buf);
                len += tmp_len; 
		buf += tmp_len; 
              
		/*
		 * Update the low bound - the next frame must always
		 * be at a higher address than the current frame.
		 */
		low = fp + 4;
		fp = frame->fp;
	} while (fp && depth--);

	return len;
}

static int sprintf_frame(struct stackframe *frame, void *buf)
{
    int len = 0, tmp_len = 0; 
    char symbol_buf[64] = { 0 }; 
#ifdef CONFIG_KALLSYMS
    tmp_len = sprintf(buf, "[<%08lx>] ", frame->pc);
    buf += tmp_len; 
    len += tmp_len; 
    sprint_symbol(symbol_buf, frame->pc);
    tmp_len = sprintf(buf, "%s", symbol_buf); 
    buf += tmp_len; 
    len += tmp_len; 

    tmp_len = sprintf(buf, " from [<%08lx>] ", frame->lr);
    buf += tmp_len; 
    len += tmp_len; 
    sprint_symbol(symbol_buf, frame->lr);
    tmp_len = sprintf(buf, "%s\n", symbol_buf); 
    buf += tmp_len; 
    len += tmp_len; 
#else
    tmp_len = sprintf(buf, "Function entered at [<%08lx>] from [<%08lx>]\n", frame->pc, frame->lr);   
    len += tmp_len; 
#endif

    return len; 

}
static int crashdump_backtrace(void *buf, struct pt_regs *regs) 
{
    unsigned long base = ((unsigned long)regs) & ~(THREAD_SIZE - 1);   
    
    return walk_stackframe(regs->ARM_fp, base, base + THREAD_SIZE, sprintf_frame, crashdump_depth, buf); 
}

static int crashdump_crashcode(void *buf, struct pt_regs *regs) 
{
	unsigned long addr = instruction_pointer(regs);
	const int thumb = thumb_mode(regs);
	const int width = thumb ? 4 : 8;
	mm_segment_t fs;
	int i;
        int tmp_len = 0, len = 0; 
	/*
	 * We need to switch to kernel mode so that we can use __get_user
	 * to safely read from kernel space.  Note that we now dump the
	 * code first, just in case the backtrace kills us.
	 */
	fs = get_fs();
	set_fs(KERNEL_DS);

	tmp_len = sprintf(buf, "Code: ");
	buf += tmp_len; 
	len += tmp_len; 

	for (i = -4; i < 1; i++) {
		unsigned int val, bad;

		if (thumb)
			bad = __get_user(val, &((u16 *)addr)[i]);
		else
			bad = __get_user(val, &((u32 *)addr)[i]);

		if (!bad){
			tmp_len = sprintf(buf, i == 0 ? "(%0*x) " : "%0*x ", width, val);
			buf += tmp_len; 
			len += tmp_len; 
		}
		else {
			tmp_len = sprintf(buf, "bad PC value.");
			buf += tmp_len; 
			len += tmp_len; 
			break;
		}
	}
	tmp_len = sprintf(buf, "\n");
	buf += tmp_len; 
	len += tmp_len; 

	set_fs(fs);
	return len; 
}

#ifdef CONFIG_PREEMPT
#define S_PREEMPT " PREEMPT"
#else
#define S_PREEMPT ""
#endif
#ifdef CONFIG_SMP
#define S_SMP " SMP"
#else
#define S_SMP ""
#endif

static uint32_t restart_reason = 0x8FFFFFFF;

static void crashdump_pm_restart(void)
{
	printk("\n reboot from crash dump! restart_reason = 0x%x !!!!\n", restart_reason);
	msm_proc_comm(PCOM_RESET_CHIP, &restart_reason, 0);

	/* go to continue for apanic*/
//	for (;;) ;
}

static int crashdump_call(struct notifier_block *nb, unsigned long val, void *data)
{
    void *dump_base = NULL; 
    void *dump_curr = NULL; 
    int len = 0; 
    int total_len = crashdump_header + crashdump_tailer; 
    struct die_args *args = (struct die_args *)data; 
    static int die_counter = 0; 
    char *stack_start = 0; 

    crashdump_printf("%s\n", __FUNCTION__); 
    if (down_interruptible(&crashdump_dev->open_sem))
    {
        return -ERESTARTSYS; 
    }

    //dump_base = ioremap(crashdump_base, crashdump_size; 
    dump_base =  crashdump_dev->dump_base;
    if (!dump_base)
    {
        crashdump_printf("ioremap failed\n"); 
        return -ENOMEM;
    }
    memset(dump_base, 0, crashdump_size); 

    *(unsigned long *)dump_base = crashdump_magic; 
    
    dump_curr = dump_base + crashdump_header; 

    /* dump errors */
    len = sprintf(dump_curr, "Internal error: %s: %lx [#%d]" S_PREEMPT S_SMP "\n", args->str, args->err, ++die_counter);
    dump_curr += len; 
    total_len += len; 
    crashdump_printf("errors len is %d\n", len); 

    /* dump modules */ 
    len = crashdump_modules(dump_curr); 
    dump_curr += len; 
    total_len += len; 
    crashdump_printf("modules len is %d\n", len); 

    /* dump registers */
    len = crashdump_registers(dump_curr, args->regs); 
    dump_curr += len; 
    total_len += len; 
    crashdump_printf("registers len is %d\n", len); 

    if (!user_mode(args->regs) || in_interrupt()) 
    {
	    /* dump stack */
	    stack_start = dump_curr; 
	    len = crashdump_stack(dump_curr, args->regs); 
	    crashdump_printf("%s\n", (char *)dump_curr); 
	    dump_curr += len; 
	    total_len += len; 
	    crashdump_printf("stack length is %d\n", len);

	    /* dump backtrace */
	    len = crashdump_backtrace(dump_curr, args->regs); 
	    crashdump_printf("%s\n", (char *)dump_curr); 
	    dump_curr += len; 
	    total_len += len; 
            crashdump_printf("backtrace len is %d\n", len); 

	    /* dump crash Code */
	    len = crashdump_crashcode(dump_curr, args->regs); 
	    crashdump_printf("%s\n", (char *)dump_curr); 
	    dump_curr += len; 
	    total_len += len; 
            crashdump_printf("crashcode len is %d\n", len); 
    }
 
    *(unsigned long *)dump_curr = crashdump_magic; 

    dump_curr = dump_base + 4; 

    for (len = 0; len < 7; len++) 
    {
        *(unsigned long *)dump_curr = total_len + len; 
	dump_curr += 4; 
    }
   
    crashdump_printf("total len is %d\n", total_len); 
    crashdump_printf("%s\n", (char *)(dump_base + crashdump_header)); 

    iounmap(dump_base); 
    
    up(&crashdump_dev->open_sem); 
	crashdump_pm_restart();
    return NOTIFY_STOP; 
}

static struct notifier_block crashdump_notifier = {
    .notifier_call = crashdump_call, 
    .priority = 1,
}; 

//static int __init mem_crashdump_init(void)
static int mem_crashdump_prob(struct platform_device *pdev)
{
    int rc = 0;
	unsigned char *mem_base = 0;
	unsigned char *fbram;
	int fbram_size;
	unsigned char *fbram_phys;

    crashdump_printf("%s\n", __FUNCTION__); 

    crashdump_dev = kzalloc(sizeof(struct mem_crashdump_dev), GFP_KERNEL); 
    if (!crashdump_dev)
    {
        crashdump_printf("Unable to alloc memory for device\n"); 
		return -ENOMEM;
    }

    /* map dump memory */
    if ((pdev->id == 0) && (pdev->num_resources > 0)) {

      //crashdump_base = pdev->resource[0].start;
      crashdump_size = pdev->resource[0].end - pdev->resource[0].start + 1;
	  mem_base = (unsigned char *)(pdev->resource[0].start);
	  
	  crashdump_printf("%s: phys addr = 0x%x, size = 0x%x\n", __FUNCTION__, mem_base, crashdump_size);
      crashdump_dev->dump_base = ioremap((unsigned long)mem_base, crashdump_size); 
	  crashdump_printf("%s: mapped virt addr = 0x%x\n", __FUNCTION__ , crashdump_dev->dump_base);
      if (!crashdump_dev->dump_base)
      {
		printk("mem_crashdump_prob failed\n");
        return -EBUSY;
      }

    }
    
    init_MUTEX(&crashdump_dev->open_sem); 

    crashdump_class = class_create(THIS_MODULE, crashdump_name); 
    if (IS_ERR(crashdump_class)) 
    {
        crashdump_printf("failed to class_create\n"); 
	goto fail_create_class; 
    }

    rc = alloc_chrdev_region(&crashdump_devno, 0, 1, crashdump_name); 
    if (rc < 0)
    {
        crashdump_printf("alloc_chrdev_region failed\n"); 
	goto fail_alloc_region; 
    }
    
    crashdump_dev->pdevice = device_create(crashdump_class, NULL, crashdump_devno, "%s", crashdump_name); 
    if (IS_ERR(crashdump_dev->pdevice)) 
    {
        crashdump_printf("device_create failed\n"); 
	goto fail_alloc_region; 
    }
    cdev_init(&crashdump_dev->cdev, &crashdump_fops); 
    crashdump_dev->cdev.owner = THIS_MODULE; 
    rc = cdev_add(&crashdump_dev->cdev, crashdump_devno, 1); 
    if (rc < 0)
    {
        crashdump_printf("failed to cdev_add"); 
	device_destroy(crashdump_class, crashdump_devno); 
	goto fail_device_create; 
    }
    else 
    {
        crashdump_dev->name = crashdump_name; 
    }
    
    register_die_notifier(&crashdump_notifier);

    return 0; 

fail_device_create: 
    unregister_chrdev_region(crashdump_devno, 1); 
fail_alloc_region:
    class_unregister(crashdump_class); 
fail_create_class:
    kfree(crashdump_dev); 
    return -EFAULT; 
}

 void mem_crashdump_exit(void)
{
    crashdump_printf("%s\n", __FUNCTION__); 
    
    unregister_die_notifier(&crashdump_notifier); 

    cdev_del(&crashdump_dev->cdev); 
    device_destroy(crashdump_class, crashdump_devno); 
    unregister_chrdev_region(crashdump_devno, 1); 
    class_unregister(crashdump_class); 
    kfree(crashdump_dev); 
}


module_param(crashdump_debug, int, S_IRWXU); 

static struct platform_driver crash_dump_driver = {
	.probe =  mem_crashdump_prob,
	.driver = {
		   .name = "crash_dump",
		   },
};


static int __init huawei_crash_init(void)
{
  return platform_driver_register(&crash_dump_driver);
}

static void __exit huawei_crash_exit(void)
{
  mem_crashdump_exit();
  platform_device_unregister(&crash_dump_driver);
}
module_init(huawei_crash_init); 
module_exit(huawei_crash_exit); 

MODULE_AUTHOR("yKF14048"); 
MODULE_DESCRIPTION("a simple kernel crash dump driver");
MODULE_VERSION("0.0.1"); 
MODULE_LICENSE("GPL"); 
