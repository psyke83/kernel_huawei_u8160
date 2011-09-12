/* kernel\drivers\video\msm\lcd_hw_debug.c
 * this file is used by the driver team to change the 
 * LCD init parameters by putting a config file in the mobile,
 * this function can make the LCD parameter debug easier.
 * 
 * Copyright (C) 2010 HUAWEI Technology Co., ltd.
 * 
 * Date: 2010/12/10
 * By genghua
 * 
 */

#include "lcd_hw_debug.h"
#include <linux/vt_kern.h>

struct sequence{
    uint32_t reg;
    uint32_t value;
    uint32_t time; //unit is ms
};

/* The following function is used to get a new line from the config file */
static char * fget_line(char * buf, int max_num, int fd,off_t *fd_seek)
{
	int cur_seek=*fd_seek;
	char c_buf[1];
	int i=0;

	for(i=0;i<max_num;i++)
	{
		buf[i]='\0';
	}
	
	sys_lseek(fd, (off_t)cur_seek,0);
	
	for(i=0;i<max_num;i++)
	{
		if ((unsigned)sys_read(fd, (char *)c_buf, 1) != 1) {
			printk(KERN_WARNING "%s: Can not read %d    \n",
				__func__, __LINE__);
			return NULL;
		}
		else
		{
			buf[i]=c_buf[0];
			cur_seek++;
			sys_lseek(fd, (off_t)cur_seek,0);
		}
		
		if(buf[i]=='\n')
		{
			break;
		}
	}
	*fd_seek=cur_seek;
	return buf;
}

/* The following function is used to malloc a region to save the new parameters */
static bool lcd_resolve_config_file(int fd,char *para_name, void **para_table,uint32_t *para_num)
{

	uint32_t reg=0;
	uint32_t val=0;
	uint32_t time=0;
	char buf[HW_LCD_CONFIGLINE_MAX] = {0};
	int lcd_config_line_num=0;
	int ret=0;
	off_t fd_seek=0;

	struct sequence * lcd_config_table = NULL;
	lcd_config_table = kzalloc( (size_t)   (sizeof(struct sequence) * HW_LCD_CONFIG_TABLE_MAX_NUM) , 0);
	
	if(NULL ==  lcd_config_table){
		goto kalloc_err;
	}
	
	
	sys_lseek(fd, (off_t)0, 0);
		
	while(NULL != (fget_line(buf, HW_LCD_CONFIGLINE_MAX, fd , &fd_seek)))
	{
		reg = 0;
	   	val= 0;
		time =0;
			
		
		ret=sscanf(buf,"{%x,%x,%d},\n",&reg,&val,&time);

		if (ret > 0 && lcd_config_line_num<HW_LCD_CONFIG_TABLE_MAX_NUM)
		{
				lcd_config_table[lcd_config_line_num].reg= reg;
				lcd_config_table[lcd_config_line_num].value= val;
				lcd_config_table[lcd_config_line_num].time= time;
				lcd_config_line_num++;
		}
		else
		{
			printk("=== resolve error, ret = %d====\n",ret);
			goto resolve_err;
		}
	}
	
	*para_num = lcd_config_line_num;
	*para_table = lcd_config_table;
	
	return TRUE;

	
resolve_err:
	kfree(lcd_config_table);
	
kalloc_err:
	
	para_table = NULL;
	*para_num = 0 ;
	return FALSE;
}

bool lcd_debug_malloc_get_para(char *para_name, void **para_table,uint32_t *para_num)
{
	int ret = 0 ;
	int fd = 0 ;
	void * table_tmp = NULL;
	int num_tmp =0 ;

	int i=0;


	if(NULL==para_table){
		return FALSE;	
	}
	
   	fd = sys_open((const char __force *) HW_LCD_INIT_TEST_PARAM, O_RDONLY, 0);

	if (fd < 0) 
	{
		printk(KERN_WARNING "%s: Can not open %s\n",
			__func__, HW_LCD_INIT_TEST_PARAM);
		return FALSE;
	}
	
	
	printk(KERN_WARNING "%s: Config file %s already opened. \n",
			__func__, HW_LCD_INIT_TEST_PARAM);
	

	//resolve the config file
	ret = lcd_resolve_config_file(fd,para_name, &table_tmp,&num_tmp);
	sys_close(fd);

	*para_table = table_tmp;
	*para_num = (uint32_t)num_tmp;
	printk("The following table is the new init parameters,you can check it with the file.\n");
	for(i=0;i<*para_num;i++)
	{
		printk("reg[%d]=%x,",i,(((struct sequence * ) table_tmp)  +i ) ->reg);
		printk("value[%d]=%x,",i,(((struct sequence *) table_tmp)  +i ) ->value);
		printk("time[%d]=%d\n",i,(((struct sequence *) table_tmp)  +i ) ->time);

	}
	
	if (FALSE == ret){
		printk("the config file has reovled error, a FALSE value is going to return.\n");	
		return FALSE;
	}
	*para_table = table_tmp;

	printk("the config file has been resolved correctly, a TRUE value is going to return.\n");	
	return TRUE;
}

bool lcd_debug_free_para(void *para_table)
{
    kfree(para_table);
	printk("The new LCD config region has been freed\n");
    return TRUE;
}
