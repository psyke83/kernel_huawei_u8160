/*
 *  linux/fs/proc/app_info.c
 *
 *
 * Changes:
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/fs.h>
#include <linux/tty.h>
#include <linux/string.h>
#include <linux/mman.h>
#include <linux/quicklist.h>
#include <linux/proc_fs.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/pagemap.h>
#include <linux/interrupt.h>
#include <linux/swap.h>
#include <linux/slab.h>
#include <linux/genhd.h>
#include <linux/smp.h>
#include <linux/signal.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/times.h>
#include <linux/profile.h>
#include <linux/utsname.h>
#include <linux/blkdev.h>
#include <linux/hugetlb.h>
#include <linux/jiffies.h>
#include <linux/sysrq.h>
#include <linux/vmalloc.h>
#include <linux/crash_dump.h>
#include <linux/pid_namespace.h>
#include <linux/bootmem.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/tlb.h>
#include <asm/div64.h>
#include "internal.h"
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <linux/hardware_self_adapt.h>

#define PROC_MANUFACTURER_STR_LEN 30
#define MAX_VERSION_CHAR 40
#define BOARD_ID_LEN 20
#define LCD_NAME_LEN 20
#define HW_VERSION   20
#define HW_VERSION_SUB_VER  6
#define SUB_VER_LEN  10

static char appsboot_version[MAX_VERSION_CHAR + 1];
static char str_flash_nand_id[PROC_MANUFACTURER_STR_LEN] = {0};
static u32 camera_id;
static u32 ts_id;
/*del 1 line*/

/* same as in proc_misc.c */
static int
proc_calc_metrics(char *page, char **start, off_t off, int count, int *eof, int len)
{
	if (len <= off + count)
		*eof = 1;
	*start = page + off;
	len -= off;
	if (len > count)
		len = count;
	if (len < 0)
		len = 0;
	return len;
}

#define ATAG_BOOT_READ_FLASH_ID 0x4d534D72
static int __init parse_tag_boot_flash_id(const struct tag *tag)
{
    char *tag_flash_id=(char*)&tag->u;
    memset(str_flash_nand_id, 0, PROC_MANUFACTURER_STR_LEN);
    memcpy(str_flash_nand_id, tag_flash_id, PROC_MANUFACTURER_STR_LEN);
    
    printk("########proc_misc.c: tag_boot_flash_id= %s\n", tag_flash_id);

    return 0;
}
__tagtable(ATAG_BOOT_READ_FLASH_ID, parse_tag_boot_flash_id);

/*parse atag passed by appsboot, ligang 00133091, 2009-4-13, start*/
#define ATAG_BOOT_VERSION 0x4d534D71 /* ATAG BOOT VERSION */
static int __init parse_tag_boot_version(const struct tag *tag)
{
    char *tag_boot_ver=(char*)&tag->u;
    memset(appsboot_version, 0, MAX_VERSION_CHAR + 1);
    memcpy(appsboot_version, tag_boot_ver, MAX_VERSION_CHAR);
     
    //printk("nand_partitions.c: appsboot_version= %s\n\n", appsboot_version);

    return 0;
}
__tagtable(ATAG_BOOT_VERSION, parse_tag_boot_version);


#define ATAG_CAMERA_ID 0x4d534D74
static int __init parse_tag_camera_id(const struct tag *tag)
{
    char *tag_boot_ver=(char*)&tag->u;
	
    memcpy((void*)&camera_id, tag_boot_ver, sizeof(u32));
     
    return 0;
}
__tagtable(ATAG_CAMERA_ID, parse_tag_camera_id);


#define ATAG_TS_ID 0x4d534D75
static int __init parse_tag_ts_id(const struct tag *tag)
{
    char *tag_boot_ver=(char*)&tag->u;
	
    memcpy((void*)&ts_id, tag_boot_ver, sizeof(u32));
     
    return 0;
}


__tagtable(ATAG_TS_ID, parse_tag_ts_id);


static int app_version_read_proc(char *page, char **start, off_t off,
				 int count, int *eof, void *data)
{
	int len;
	char *ker_ver = HUAWEI_KERNEL_VERSION;
	char *lcd_name = NULL;
	char s_board_id[BOARD_ID_LEN] = {0};
	char sub_ver[SUB_VER_LEN] = {0};
	char hw_version_id[HW_VERSION] = {0};
	char hw_version_sub_ver[HW_VERSION_SUB_VER] = {0};	

	switch(machine_arch_type)
	{
		case MACH_TYPE_MSM7X25_U8100:
			strcpy(s_board_id, "MSM7X25_U8100");
			break;
		case MACH_TYPE_MSM7X25_U8105:
			strcpy(s_board_id, "MSM7X25_U8105");
			break;
		case MACH_TYPE_MSM7X25_U8107:
			strcpy(s_board_id, "MSM7X25_U8107");
			break;
		case MACH_TYPE_MSM7X25_U8109:
			strcpy(s_board_id, "MSM7X25_U8109");
			break;
		case MACH_TYPE_MSM7X25_U8110:
			strcpy(s_board_id, "MSM7X25_U8110");
			break;
		/*del U8115 and U8117*/
		case MACH_TYPE_MSM7X25_U8120:
			strcpy(s_board_id, "MSM7X25_U8120");
			break;
		case MACH_TYPE_MSM7X25_U7610:
			strcpy(s_board_id, "MSM7X25_U7610");
			break;
		case MACH_TYPE_MSM7X25_U8500:
			strcpy(s_board_id, "MSM7X25_U8500");
			break;
		case MACH_TYPE_MSM7X25_UM840:
			strcpy(s_board_id, "MSM7X25_UM840");
			break;
		case MACH_TYPE_MSM7X25_U8300:
			strcpy(s_board_id, "MSM7X25_U8300");
			break;
		case MACH_TYPE_MSM7X25_U8350:
			strcpy(s_board_id, "MSM7X25_U8350");
			
			strcpy(hw_version_id, "HD1U835M ");
			sprintf(hw_version_sub_ver, "VER.%c", 'A'+(char)get_hw_sub_board_id());
			break;
		/*U8150_EU and U8150_JP*/
		case MACH_TYPE_MSM7X25_U8150:
			strcpy(s_board_id, "MSM7X25_U8150");
			
			strcpy(hw_version_id, "HD1U815M ");
			
			/*if sub_board_id is equal to 0(VerA), the product is HD1U815M VER.C*/
			if(HW_VER_SUB_VA == get_hw_sub_board_id())
			{
				sprintf(hw_version_sub_ver, "VER.%c", 'C');
			}
			else if(HW_VER_SUB_VB == get_hw_sub_board_id())
			{
				sprintf(hw_version_sub_ver, "VER.%c", 'C');
			}
			else if(HW_VER_SUB_VC == get_hw_sub_board_id())
			{
				sprintf(hw_version_sub_ver, "VER.%c", 'D');
			}
			else if(HW_VER_SUB_VD == get_hw_sub_board_id())
			{
				sprintf(hw_version_sub_ver, "VER.%c", 'D');
			}
			else
			{
				sprintf(hw_version_sub_ver, "VER.%c", 'A'+(char)get_hw_sub_board_id());
			}
			break;
		case MACH_TYPE_MSM7X25_U8159:
			strcpy(s_board_id, "MSM7X25_U8159");

			strcpy(hw_version_id, "HD1U815M ");
			sprintf(hw_version_sub_ver, "VER.%c", 'D');
			break;
		case MACH_TYPE_MSM7X25_U8160:
			strcpy(s_board_id, "MSM7X25_U8160");
			strcpy(hw_version_id, "HD1U813M ");
			if(HW_VER_SUB_VA == get_hw_sub_board_id())
			{
				sprintf(hw_version_sub_ver, "VER.%c", 'A');
			}
			else
			{
				sprintf(hw_version_sub_ver, "VER.%c", 'C');
			}
			break;
		case MACH_TYPE_MSM7X25_U8130:
			strcpy(s_board_id, "MSM7X25_U8180");
			strcpy(hw_version_id, "HD1U813M ");
			sprintf(hw_version_sub_ver, "VER.%c", 'C');
			break;
		case MACH_TYPE_MSM7X25_C8510:
			strcpy(s_board_id, "MSM7X25_C8510");
			strcpy(hw_version_id, "HC1C851M ");
			sprintf(hw_version_sub_ver, "VER.%c", 'A'+(char)get_hw_sub_board_id());
			break;
		case MACH_TYPE_MSM7X25_C8500:
			strcpy(s_board_id, "MSM7X25_C8500");
			break;
	    case MACH_TYPE_MSM7X25_C8600:
			strcpy(s_board_id, "MSM7X25_C8600");
			break;
		case MACH_TYPE_MSM7X25_C8150:
			strcpy(s_board_id, "MSM7X25_C8150");
			
			if(HW_VER_SUB_VD == get_hw_sub_board_id())
			{
				/*C8150 VD will show Ver.C*/
				strcpy(hw_version_id, "HC1C815M ");
				sprintf(hw_version_sub_ver, "VER.%c", 'C');
			}
			else if(HW_VER_SUB_VF == get_hw_sub_board_id())
			{
				/*C8150 VF is C8150-1.VER.C product*/
				strcpy(hw_version_id, "HC1C815M ");
				sprintf(hw_version_sub_ver, "VER.%c", 'C');
			}
			else if(HW_VER_SUB_VG == get_hw_sub_board_id())
			{
                /* reinit s_board_id to M835 */
                memset(s_board_id, 0, BOARD_ID_LEN);
			    strcpy(s_board_id, "MSM7X25_M835");
				strcpy(hw_version_id, "HC1C815M ");
			}
			else
			{
				strcpy(hw_version_id, "HC1C815M ");
				sprintf(hw_version_sub_ver, "VER.%c", 'A'+(char)get_hw_sub_board_id());
			}
			break;
		case MACH_TYPE_MSM7X25_M860:
			strcpy(s_board_id, "MSM7X25_M860");
			strcpy(hw_version_id, "HC1M860M");
			break;

		default:
			strcpy(s_board_id, "ERROR");
			break;
	}

    if(MACH_TYPE_MSM7X25_U8120 == machine_arch_type)
    {
        sprintf(sub_ver, ".Ver%c", 'B');
    }
	/* del C8150 branch, using else branch */
    /* U8150_EU:U8150.VerA/VerC and U8150_JP:U8150.VerB/VerD */
    else if(MACH_TYPE_MSM7X25_U8150 == machine_arch_type)
    {
        /*if sub_board_id is equal to 0(VerA), the product is U8150_EU.VerC */
        if(HW_VER_SUB_VA == get_hw_sub_board_id())
        {
            sprintf(sub_ver, "_EU.Ver%c", 'C');
        }
        /*if sub_board_id is equal to 1(VerB), the product is U8150_JP.VerC */
        else if(HW_VER_SUB_VB == get_hw_sub_board_id())
        {
            sprintf(sub_ver, "_JP.Ver%c", 'C');
        }
        /*if sub_board_id is equal to 2(VerC), the product is U8150_EU.VerD */
        else if(HW_VER_SUB_VC == get_hw_sub_board_id())
        {
            sprintf(sub_ver, "_EU.Ver%c", 'D');
        }
        /*if sub_board_id is equal to 3(VerD), the product is U8150_JP.VerD */
        else if(HW_VER_SUB_VD == get_hw_sub_board_id())
        {
            sprintf(sub_ver, "_JP.Ver%c", 'D');
        }
        else
        {
            sprintf(sub_ver, ".Ver%c", 'A'+(char)get_hw_sub_board_id());
        }
    }
    else if(MACH_TYPE_MSM7X25_U8500 == machine_arch_type)
    {
        /*if sub_board_id is equal to 0(VerA), the product is U8150_EU.VerA */
        if(HW_VER_SUB_VA == get_hw_sub_board_id())
        {
            sprintf(sub_ver, ".Ver%c", 'A');
        }
        else
        {
            sprintf(sub_ver, ".Ver%c", 'C');
        }
    }
    else if(MACH_TYPE_MSM7X25_M860 == machine_arch_type)
    {
        sub_ver[0] = 0;
        if(get_hw_sub_board_id() >= HW_VER_SUB_VC)
        {
            sprintf(sub_ver, ".Ver%c", 'A'+(char)get_hw_sub_board_id());
        }

    }
	else if(MACH_TYPE_MSM7X25_U8130 == machine_arch_type)
	{
		sub_ver[0] = 0;
		sprintf(sub_ver, ".Ver%c", 'C');
	}
	else if(MACH_TYPE_MSM7X25_U8160 == machine_arch_type)
	{
		sub_ver[0] = 0;
		if(HW_VER_SUB_VA == get_hw_sub_board_id())
		{
			sprintf(sub_ver, ".Ver%c", 'A');
		}
		else
		{
			sprintf(sub_ver, ".Ver%c", 'C');
		}
	}
    else
    {
        sprintf(sub_ver, ".Ver%c", 'A'+(char)get_hw_sub_board_id());
    }
    strcat(s_board_id, sub_ver);

  
    strcat(hw_version_id, hw_version_sub_ver);
   
	lcd_name = get_lcd_panel_name();
	
	len = snprintf(page, PAGE_SIZE, "APPSBOOT:\n"
	"%s\n"
	"KERNEL_VER:\n"
	"%s\n"
	 "FLASH_ID:\n"
	"%s\n"
	"board_id:\n%s\n"
	"lcd_id:\n%s\n"
	"cam_id:\n%d\n"
	"ts_id:\n%d\n"
	"hw_version:\n%s\n",
	appsboot_version, ker_ver, str_flash_nand_id, s_board_id, lcd_name, camera_id, ts_id, hw_version_id);
	
	return proc_calc_metrics(page, start, off, count, eof, len);
}

void __init proc_app_info_init(void)
{
	static struct {
		char *name;
		int (*read_proc)(char*,char**,off_t,int,int*,void*);
	} *p, simple_ones[] = {
		
        {"app_info", app_version_read_proc},
		{NULL,}
	};
	for (p = simple_ones; p->name; p++)
		create_proc_read_entry(p->name, 0, NULL, p->read_proc, NULL);

}


