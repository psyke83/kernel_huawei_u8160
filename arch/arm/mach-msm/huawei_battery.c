/* arch/arm/mach-msm/huawei_battery.c
 *
 * Copyright (C) 2009 HUAWEI Corporation.
 * Copyright (C) 2009 Google, Inc.
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <mach/board.h>
#include <linux/debugfs.h>
#include <linux/wakelock.h>
#include <mach/msm_rpcrouter.h>

#ifdef CONFIG_HUAWEI_EVALUATE_POWER_CONSUMPTION
#include <mach/huawei_battery.h>
#endif


#ifdef CONFIG_HUAWEI_BATTERY
#define TRACE_BATT 1
static int batt_debug_mask = 0;
module_param_named(debug_mask, batt_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

#if defined(CONFIG_HUAWEI_KERNEL)
#define BATT(x...) do {if (printk_ratelimit() && (0 != batt_debug_mask)) printk(KERN_INFO "[BATT] " x); } while (0)
#else
#if TRACE_BATT
#define BATT(x...) printk(KERN_INFO "[BATT] " x)
#else
#define BATT(x...) do {} while (0)
#endif
#endif
#define HEALTH_TEMP_MAX 60       /* define temperature max,and decide whether it's overheat  */
#define HEALTH_TEMP_MIN (-20)       /* define temperature min,and decide whether it's over cold */
#define HEALTH_VOLT_MAX 4250     /* define voltage max,and decide whether it's overvoltage  */
#define TRUE 1
#define FALSE 0
/* batt client definitions  */
#define APP_BATT_PROG			0x30000093
#define APP_BATT_VER			0x00000011
#define HUAWEI_PROCEDURE_BATTERY_NULL	0
#define HUAWEI_PROCEDURE_GET_BATT_INFO	2
#define HUAWEI_PROCEDURE_SET_BATT_DELTA	1
#define HUAWEI_PROCEDURE_GET_POWER_DOWN_CHARGING_FLAG 3
#define SUSPEND_DELTA_LEVEL 20     /* define delta level in suspend  */
#define RESUME_DELTA_LEVEL  1     /* define delta level in resume  */

#ifdef CONFIG_HUAWEI_EVALUATE_POWER_CONSUMPTION
#define HUAWEI_PROCEDURE_CONSUEM_CURRENT_NTIFY 4
#endif
#define HUAWEI_BAT_DISP_FULL_LEVEL_VALUE 90

typedef enum
{
    CHARGER_BATTERY = 0,
    CHARGER_USB,
    CHARGER_AC,
    CHARGER_INVALID
} charger_type_t;

struct battery_info_reply
{
    u32 batt_id;                     /* Battery ID from ADC */
    u32 batt_vol;                 /* Battery voltage from ADC */
    u32 batt_temp;             /* Battery Temperature (C) from formula and ADC */
    u32 batt_current;          /* Battery current from ADC */
    u32 level;                    /* formula */
    u32 charging_source;           /* 0: no cable, 1:usb, 2:AC */
    u32 charging_enabled;    /* 0: Disable, 1: Enable */
    u32 full_bat;                     /* Full capacity of battery (mAh) */
    u32 batt_present;
    u32 charging_event;
    u32 power_off_charging;

};

struct huawei_battery_info
{
    int present;
	/* lock to protect the battery info */
	struct mutex lock;
	/* lock held while calling the arm9 to query the battery info */
	struct mutex rpc_lock;
    struct battery_info_reply rep;
    int charging_status;
    int charger;
};
static struct wake_lock low_level_wake_lock;
static struct msm_rpc_endpoint *endpoint;
static struct huawei_battery_info huawei_batt_info;
static int huawei_battery_initial = 0;

static enum power_supply_property huawei_battery_properties[] =
{
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_TECHNOLOGY,
    POWER_SUPPLY_PROP_CAPACITY,
};

static enum power_supply_property huawei_power_properties[] = {
    POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_list[] = {
    "battery",
};

static int huawei_power_get_property(struct power_supply *psy, 
				    enum power_supply_property psp,
				    union power_supply_propval *val);

static int huawei_battery_get_property(struct power_supply *psy, 
				    enum power_supply_property psp,
				    union power_supply_propval *val);

static struct power_supply huawei_power_supplies[] =
{
    {
        .name = "battery",
        .type = POWER_SUPPLY_TYPE_BATTERY,
        .properties = huawei_battery_properties,
        .num_properties = ARRAY_SIZE(huawei_battery_properties),
        .get_property = huawei_battery_get_property,
    },
    {
        .name = "usb",
        .type = POWER_SUPPLY_TYPE_USB,
        .supplied_to = supply_list,
        .num_supplicants = ARRAY_SIZE(supply_list),
        .properties = huawei_power_properties,
        .num_properties = ARRAY_SIZE(huawei_power_properties),
        .get_property = huawei_power_get_property,
    },
    {
        .name = "ac",
        .type = POWER_SUPPLY_TYPE_MAINS,
        .supplied_to = supply_list,
        .num_supplicants = ARRAY_SIZE(supply_list),
        .properties = huawei_power_properties,
        .num_properties = ARRAY_SIZE(huawei_power_properties),
        .get_property = huawei_power_get_property,
    },
};



typedef enum
{
    CHG_IDLE_ST,                                          /* Charger state machine entry point.       */
    CHG_WALL_IDLE_ST,                               /* Wall charger state machine entry point.  */
    CHG_WALL_TRICKLE_ST,                          /* Wall charger low batt charging state.    */
    CHG_WALL_NO_TX_FAST_ST,                 /* Wall charger high I charging state.      */
    CHG_WALL_FAST_ST,                              /* Wall charger high I charging state.      */
    CHG_WALL_TOPOFF_ST,                         /* Wall charger top off charging state.     */
    CHG_WALL_MAINT_ST,                           /* Wall charger maintance charging state.   */
    CHG_WALL_TX_WAIT_ST,                       /* Wall charger TX WAIT charging state.     */
    CHG_WALL_ERR_WK_BAT_WK_CHG_ST, /* Wall CHG ERR: weak batt and weak charger.*/
    CHG_WALL_ERR_WK_BAT_BD_CHG_ST,  /* Wall CHG ERR: weak batt and bad charger. */
    CHG_WALL_ERR_GD_BAT_BD_CHG_ST,  /* Wall CHG ERR: good batt and bad charger. */
    CHG_WALL_ERR_GD_BAT_WK_CHG_ST, /* Wall CHG ERR: good batt and weak charger.*/
    CHG_WALL_ERR_BD_BAT_GD_CHG_ST, /* Wall CHG ERR: Bad batt and good charger. */
    CHG_WALL_ERR_BD_BAT_WK_CHG_ST,/* Wall CHG ERR: Bad batt and weak charger. */
    CHG_WALL_ERR_BD_BAT_BD_CHG_ST,/* Wall CHG ERR: Bad batt and bad charger.  */
    CHG_WALL_ERR_GD_BAT_BD_BTEMP_CHG_ST,/* Wall CHG ERR: GD batt and BD batt temp */
    CHG_WALL_ERR_WK_BAT_BD_BTEMP_CHG_ST,/* Wall CHG ERR: WK batt and BD batt temp */
    CHG_USB_IDLE_ST,                                            /* USB charger state machine entry point.   */
    CHG_USB_TRICKLE_ST,                                      /* USB charger low batt charging state.     */
    CHG_USB_NO_TX_FAST_ST,                              /* USB charger high I charging state.       */
    CHG_USB_FAST_ST,                                          /* USB charger high I charging state.       */
    CHG_USB_TOPOFF_ST,                                     /* USB charger top off state charging state.*/
    CHG_USB_DONE_ST,                                         /* USB charger Done charging state.         */
    CHG_USB_ERR_WK_BAT_WK_CHG_ST,     /* USB CHG ERR: weak batt and weak charger. */
    CHG_USB_ERR_WK_BAT_BD_CHG_ST,     /* USB CHG ERR: weak batt and bad charger.  */
    CHG_USB_ERR_GD_BAT_BD_CHG_ST,     /* USB CHG ERR: good batt and bad charger.  */
    CHG_USB_ERR_GD_BAT_WK_CHG_ST,    /* USB CHG ERR: good batt and weak charger. */
    CHG_USB_ERR_BD_BAT_GD_CHG_ST,    /* USB CHG ERR: Bad batt and good charger.  */
    CHG_USB_ERR_BD_BAT_WK_CHG_ST,   /* USB CHG ERR: Bad batt and weak charger.  */
    CHG_USB_ERR_BD_BAT_BD_CHG_ST,   /* USB CHG ERR: Bad batt and bad charger.   */
    CHG_USB_ERR_GD_BAT_BD_BTEMP_CHG_ST,/* USB CHG ERR: GD batt and BD batt temp */
    CHG_USB_ERR_WK_BAT_BD_BTEMP_CHG_ST,/* USB CHG ERR: WK batt and BD batt temp */
    CHG_VBATDET_CAL_ST,                                  /* VBATDET calibration state*/
    CHG_INVALID_ST
} chg_state_type;

static int get_usb_ac_charging_status(chg_state_type charging_status_para)
{
    int result_val;
    static int last_result_val = POWER_SUPPLY_STATUS_UNKNOWN;
    switch (charging_status_para)
    {
        case CHG_WALL_IDLE_ST:            /* Wall charger state machine entry point.  */
        case CHG_WALL_TRICKLE_ST:             /* Wall charger low batt charging state.    */
        case CHG_WALL_NO_TX_FAST_ST:       /* Wall charger high I charging state.      */
        case CHG_WALL_FAST_ST:                  /* Wall charger high I charging state.      */
        case CHG_WALL_TOPOFF_ST:               /* Wall charger top off charging state.     */
        case CHG_WALL_TX_WAIT_ST:            /* Wall charger TX WAIT charging state.     */
        case CHG_USB_IDLE_ST:              /* USB charger state machine entry point.   */
        case CHG_USB_TRICKLE_ST:              /* USB charger low batt charging state.     */
        case CHG_USB_NO_TX_FAST_ST:        /* USB charger high I charging state.       */
        case CHG_USB_FAST_ST:                   /* USB charger high I charging state.       */
        case CHG_USB_TOPOFF_ST:               /* USB charger top off state charging state.*/
            result_val = POWER_SUPPLY_STATUS_CHARGING;
            break;
            
        case CHG_IDLE_ST:                     /* Charger state machine entry point.  */
            result_val = POWER_SUPPLY_STATUS_DISCHARGING;
            break;
            
        case CHG_VBATDET_CAL_ST:      /* VBATDET calibration state*/
            return last_result_val;
            
        case CHG_WALL_MAINT_ST:                /* Wall charger maintance charging state.  */
        case CHG_USB_DONE_ST:                   /* USB charger Done charging state.   */
            result_val = POWER_SUPPLY_STATUS_FULL;
            break;
            
        case CHG_WALL_ERR_WK_BAT_WK_CHG_ST:             /* Wall CHG ERR: weak batt and weak charger.*/
        case CHG_WALL_ERR_WK_BAT_BD_CHG_ST:             /* Wall CHG ERR: weak batt and bad charger. */
        case CHG_WALL_ERR_GD_BAT_BD_CHG_ST:              /* Wall CHG ERR: good batt and bad charger. */
        case CHG_WALL_ERR_GD_BAT_WK_CHG_ST:              /* Wall CHG ERR: good batt and weak charger.*/
        case CHG_WALL_ERR_BD_BAT_GD_CHG_ST:              /*   Wall CHG ERR: Bad batt and good charger. */
        case CHG_WALL_ERR_BD_BAT_WK_CHG_ST:              /*   Wall CHG ERR: Bad batt and weak charger. */
        case CHG_WALL_ERR_BD_BAT_BD_CHG_ST:              /*   Wall CHG ERR: Bad batt and bad charger.  */
        case CHG_WALL_ERR_GD_BAT_BD_BTEMP_CHG_ST:   /*   Wall CHG ERR: GD batt and BD batt temp */
        case CHG_WALL_ERR_WK_BAT_BD_BTEMP_CHG_ST:   /* Wall CHG ERR: WK batt and BD batt temp */
        case CHG_USB_ERR_WK_BAT_WK_CHG_ST:               /* USB CHG ERR: weak batt and weak charger. */
        case CHG_USB_ERR_WK_BAT_BD_CHG_ST:               /* USB CHG ERR: weak batt and bad charger.  */
        case CHG_USB_ERR_GD_BAT_BD_CHG_ST:               /* USB CHG ERR: good batt and bad charger.  */
        case CHG_USB_ERR_GD_BAT_WK_CHG_ST:              /* USB CHG ERR: good batt and weak charger. */
        case CHG_USB_ERR_BD_BAT_GD_CHG_ST:              /*  USB CHG ERR: Bad batt and good charger.  */
        case CHG_USB_ERR_BD_BAT_WK_CHG_ST:             /* USB CHG ERR: Bad batt and weak charger.  */
        case CHG_USB_ERR_BD_BAT_BD_CHG_ST:              /* USB CHG ERR: Bad batt and bad charger.   */
        case CHG_USB_ERR_GD_BAT_BD_BTEMP_CHG_ST:  /* USB CHG ERR: GD batt and BD batt temp */
        case CHG_USB_ERR_WK_BAT_BD_BTEMP_CHG_ST:  /*   USB CHG ERR: WK batt and BD batt temp */
            result_val = POWER_SUPPLY_STATUS_NOT_CHARGING;
            break;
            
        case CHG_INVALID_ST:
            result_val = POWER_SUPPLY_STATUS_UNKNOWN;
            break;
            
        default:
            result_val = POWER_SUPPLY_STATUS_UNKNOWN;
    }
    last_result_val = result_val;
    return result_val;
}


static int get_power_source(chg_state_type charging_status_parameter, u32 present_para)
{
    int result_value;
    static int last_result_value = CHARGER_INVALID;
    switch (charging_status_parameter)
    {
        case CHG_WALL_IDLE_ST:                               /* Wall charger state machine entry point.  */
        case CHG_WALL_TRICKLE_ST:             /* Wall charger low batt charging state.    */
        case CHG_WALL_NO_TX_FAST_ST:       /* Wall charger high I charging state.      */
        case CHG_WALL_FAST_ST:                  /* Wall charger high I charging state.      */
        case CHG_WALL_TOPOFF_ST:               /* Wall charger top off charging state.     */
        case CHG_WALL_TX_WAIT_ST:            /* Wall charger TX WAIT charging state.     */
        case CHG_WALL_MAINT_ST:                /* Wall charger maintance charging state.  */
        case CHG_WALL_ERR_WK_BAT_WK_CHG_ST:             /* Wall CHG ERR: weak batt and weak charger.*/
        case CHG_WALL_ERR_WK_BAT_BD_CHG_ST:             /* Wall CHG ERR: weak batt and bad charger. */
        case CHG_WALL_ERR_GD_BAT_BD_CHG_ST:              /* Wall CHG ERR: good batt and bad charger. */
        case CHG_WALL_ERR_GD_BAT_WK_CHG_ST:              /* Wall CHG ERR: good batt and weak charger.*/
        case CHG_WALL_ERR_BD_BAT_GD_CHG_ST:              /*   Wall CHG ERR: Bad batt and good charger. */
        case CHG_WALL_ERR_BD_BAT_WK_CHG_ST:              /*   Wall CHG ERR: Bad batt and weak charger. */
        case CHG_WALL_ERR_BD_BAT_BD_CHG_ST:              /*   Wall CHG ERR: Bad batt and bad charger.  */
        case CHG_WALL_ERR_GD_BAT_BD_BTEMP_CHG_ST:   /*   Wall CHG ERR: GD batt and BD batt temp */
        case CHG_WALL_ERR_WK_BAT_BD_BTEMP_CHG_ST:   /* Wall CHG ERR: WK batt and BD batt temp */
            result_value = CHARGER_AC;
            break;
            
        case CHG_USB_IDLE_ST:                                            /* USB charger state machine entry point.   */
        case CHG_USB_TRICKLE_ST:              /* USB charger low batt charging state.     */
        case CHG_USB_NO_TX_FAST_ST:        /* USB charger high I charging state.       */
        case CHG_USB_FAST_ST:                   /* USB charger high I charging state.       */
        case CHG_USB_TOPOFF_ST:               /* USB charger top off state charging state.*/
        case CHG_USB_DONE_ST:                   /* USB charger Done charging state.   */
        case CHG_USB_ERR_WK_BAT_WK_CHG_ST:               /* USB CHG ERR: weak batt and weak charger. */
        case CHG_USB_ERR_WK_BAT_BD_CHG_ST:               /* USB CHG ERR: weak batt and bad charger.  */
        case CHG_USB_ERR_GD_BAT_BD_CHG_ST:               /* USB CHG ERR: good batt and bad charger.  */
        case CHG_USB_ERR_GD_BAT_WK_CHG_ST:              /* USB CHG ERR: good batt and weak charger. */
        case CHG_USB_ERR_BD_BAT_GD_CHG_ST:              /*  USB CHG ERR: Bad batt and good charger.  */
        case CHG_USB_ERR_BD_BAT_WK_CHG_ST:             /* USB CHG ERR: Bad batt and weak charger.  */
        case CHG_USB_ERR_BD_BAT_BD_CHG_ST:              /* USB CHG ERR: Bad batt and bad charger.   */
        case CHG_USB_ERR_GD_BAT_BD_BTEMP_CHG_ST:  /* USB CHG ERR: GD batt and BD batt temp */
        case CHG_USB_ERR_WK_BAT_BD_BTEMP_CHG_ST:  /*   USB CHG ERR: WK batt and BD batt temp */
            result_value = CHARGER_USB;
            break;
            
        case CHG_VBATDET_CAL_ST:              /* VBATDET calibration state*/
            return last_result_value;
            
        default:
            if (present_para)
            {
                result_value = CHARGER_BATTERY;
            }
            else
            {
                result_value = CHARGER_INVALID;
            }
    }
    last_result_value = result_value;
    return result_value;
}

static int huawei_battery_get_charging_status(void)
{
    u32 level;
    charger_type_t charger;
    int ret;
    int usb_ac_ret;
    charger = huawei_batt_info.charger;
    usb_ac_ret = huawei_batt_info.charging_status;
    switch (charger)
    {
        case CHARGER_BATTERY:
            ret = POWER_SUPPLY_STATUS_DISCHARGING;
            break;
        case CHARGER_USB:
        case CHARGER_AC:
            level = huawei_batt_info.rep.level;
            if (level == 100)
            {
                ret = POWER_SUPPLY_STATUS_FULL;
            }
            else
            {
                ret = POWER_SUPPLY_STATUS_CHARGING;
            }

            break;
        default:
            ret = POWER_SUPPLY_STATUS_UNKNOWN;
    }

    if ((POWER_SUPPLY_STATUS_FULL == ret) || (POWER_SUPPLY_STATUS_FULL == usb_ac_ret))
    {
        usb_ac_ret = POWER_SUPPLY_STATUS_FULL;
    }

    return usb_ac_ret;
    
}

static int huawei_battery_status_update(u32 curr_level, int curr_temp, u32 curr_voltage)
{
	if (!huawei_battery_initial)
		return 0;
    mutex_lock(&huawei_batt_info.lock);
	huawei_batt_info.rep.level = curr_level;
    huawei_batt_info.rep.batt_temp = curr_temp;
    huawei_batt_info.rep.batt_vol = curr_voltage;
    if (POWER_SUPPLY_STATUS_FULL == huawei_batt_info.charging_status)
    {
        /*如果长时间大电流应用的情况下导致电池没有充满，则显示实际值*/
        if(curr_level >= HUAWEI_BAT_DISP_FULL_LEVEL_VALUE)
        {
            huawei_batt_info.rep.level = 100;
        }
    }
	mutex_unlock(&huawei_batt_info.lock);
    if (huawei_batt_info.rep.level < 2) 
    {
        wake_lock_timeout(&low_level_wake_lock, 60*HZ);
    }
	power_supply_changed(&huawei_power_supplies[CHARGER_BATTERY]);
	return 0;
}

static int huawei_charging_event_status_update(int charging_status, int charger)
{
	int notify;

	/* bug fix for sync issue between ARM9 and ARM11*/
	/*if (!huawei_battery_initial)
		return 0;*/

	mutex_lock(&huawei_batt_info.lock);
	notify = ((huawei_batt_info.charging_status != charging_status)||
                           (huawei_batt_info.charger != charger));
	huawei_batt_info.charging_status = charging_status;
    huawei_batt_info.charger = charger;
	mutex_unlock(&huawei_batt_info.lock);
	
	/* bug fix for sync issue between ARM9 and ARM11*/
	if (!huawei_battery_initial)
		return 0;

	if (notify)
	{
		power_supply_changed(&huawei_power_supplies[CHARGER_BATTERY]);
        power_supply_changed(&huawei_power_supplies[CHARGER_USB]);
        power_supply_changed(&huawei_power_supplies[CHARGER_AC]);
	}
    return 0;
}

static int huawei_get_power_down_charging_flag(u32 *buffer)
{
	
	struct rpc_request_hdr req;
	
	struct huawei_get_batt_info_rep {
		struct rpc_reply_hdr hdr;
		uint32_t result;
	} rep;

	int rc;
	uint32_t ntohl32_result;
	if (buffer == NULL) 
		return -EINVAL;

	rc = msm_rpc_call_reply(endpoint, HUAWEI_PROCEDURE_GET_POWER_DOWN_CHARGING_FLAG,
				&req, sizeof(req),
				&rep, sizeof(rep),
				5 * HZ);
 
	ntohl32_result = be32_to_cpu(rep.result);
	BATT("huawei_get_power_down_charging_flag , ntohl32_result = %d\n", ntohl32_result);
	if ( rc < 0 ) 
		return rc;

	BATT("huawei_get_power_down_charging_flag , RPC SUCC... \n");
    *buffer = ntohl32_result;		 
	return 0;
	
}

static int huawei_battery_get_battery_health(void)
{
    int health_present;
    int health_temp;
    int health_volt;
    int relt;

    health_present = huawei_batt_info.present;
    health_temp = huawei_batt_info.rep.batt_temp;
    health_volt = huawei_batt_info.rep.batt_vol;
    if (!health_present)
    {
        relt = POWER_SUPPLY_HEALTH_UNKNOWN;
    }
    else if (health_temp / 10 > HEALTH_TEMP_MAX)
    {
        relt = POWER_SUPPLY_HEALTH_OVERHEAT;
    }
    /*增加过低温的判断*/
    else if (health_temp / 10 < HEALTH_TEMP_MIN)
    {
        relt = POWER_SUPPLY_HEALTH_COLD;
    }    
    else if (health_volt > HEALTH_VOLT_MAX)
    {
        relt = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
    }
    else
    {
        relt = POWER_SUPPLY_HEALTH_GOOD;
    }

    return relt;
}

static int huawei_battery_get_property(struct power_supply *       psy,
                                       enum power_supply_property  psp,
                                       union power_supply_propval *val)
{
    switch (psp)
    {
        case POWER_SUPPLY_PROP_STATUS:
		val->intval = huawei_battery_get_charging_status();
            break;
        case POWER_SUPPLY_PROP_HEALTH:
            val->intval = huawei_battery_get_battery_health();
            break;
        case POWER_SUPPLY_PROP_TECHNOLOGY:
            val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
            break;
        case POWER_SUPPLY_PROP_PRESENT:
            val->intval = huawei_batt_info.present;
            break;
        case POWER_SUPPLY_PROP_CAPACITY:
            mutex_lock(&huawei_batt_info.lock);
            val->intval = huawei_batt_info.rep.level;
            mutex_unlock(&huawei_batt_info.lock);
            break;
        default:
            return -EINVAL;
    }

    return 0;
}

static int huawei_power_get_property(struct power_supply *       psy,
                                     enum power_supply_property  psp,
                                     union power_supply_propval *val)
{
    charger_type_t charger;
    charger = huawei_batt_info.charger;
    switch (psp)
    {
        case POWER_SUPPLY_PROP_ONLINE:
            if (psy->type == POWER_SUPPLY_TYPE_MAINS)
            {
                val->intval = (charger == CHARGER_AC ? 1 : 0);
            }
            else if (psy->type == POWER_SUPPLY_TYPE_USB)
            {
                val->intval = (charger == CHARGER_USB ? 1 : 0);
            }
            else
            {
                val->intval = 0;
            }

            break;
        default:
            return -EINVAL;
    }
    return 0;
}

enum
{
    BATT_ID = 0,
    BATT_LEVEL,
    BATT_VOL,
    BATT_TEMP,
    BATT_CURRENT,
    CHARGING_SOURCE,
    CHARGING_ENABLED,
    FULL_BAT,
    CHARGING_EVENT,
    BATT_PRESENT,
    POWER_OFF_CHARGING,
};

static ssize_t huawei_battery_show_property(struct device *          dev,
                                            struct device_attribute *attr,
                                            char *                   buf);

#define HUAWEI_BATTERY_ATTR(_name) \
    {                                       \
        .attr = { .name = # _name, .mode = S_IRUGO, .owner = THIS_MODULE }, \
        .show  = huawei_battery_show_property, \
        .store = NULL, \
                                                             }

static struct device_attribute huawei_battery_attrs[] =
{
    HUAWEI_BATTERY_ATTR(batt_id),
    HUAWEI_BATTERY_ATTR(level),
    HUAWEI_BATTERY_ATTR(batt_vol),
    HUAWEI_BATTERY_ATTR(batt_temp),
    HUAWEI_BATTERY_ATTR(batt_current),
    HUAWEI_BATTERY_ATTR(charging_source),
    HUAWEI_BATTERY_ATTR(charging_enabled),
    HUAWEI_BATTERY_ATTR(full_bat),
    HUAWEI_BATTERY_ATTR(charging_event),
    HUAWEI_BATTERY_ATTR(batt_present),
    HUAWEI_BATTERY_ATTR(power_off_charging),
};

#ifdef CONFIG_HUAWEI_EVALUATE_POWER_CONSUMPTION
int huawei_rpc_current_consuem_notify(device_current_consume_type device_event, __u32 device_state)
{
	struct set_consume_notify_req {
		struct rpc_request_hdr hdr;
		uint32_t device_event;
		uint32_t device_state;
	} req;
	req.device_event = cpu_to_be32(device_event);
    req.device_state = cpu_to_be32(device_state);
	if (IS_ERR(endpoint)) {
        printk(KERN_ERR "%s: rpc not initilized! rc = %ld\n",
               __FUNCTION__, PTR_ERR(endpoint));
        return 0;
    }
    BATT("%s: device_event= %d , device_state= %d \n",__FUNCTION__,device_event,device_state);
	return msm_rpc_call(endpoint, HUAWEI_PROCEDURE_CONSUEM_CURRENT_NTIFY,
			    &req, sizeof(req), 5 * HZ);
}

#endif

/*设定battery上报的门限*/
static int huawei_rpc_set_delta(unsigned delta)
{
	struct set_batt_delta_req {
		struct rpc_request_hdr hdr;
		uint32_t data;
	} req;

	req.data = cpu_to_be32(delta);
	return msm_rpc_call(endpoint, HUAWEI_PROCEDURE_SET_BATT_DELTA,
			    &req, sizeof(req), 5 * HZ);
}

static ssize_t huawei_battery_set_delta(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int rc;
	unsigned long delta = 0;
	
	delta = simple_strtoul(buf, NULL, 10);

	if (delta > 100)
		return -EINVAL;

	mutex_lock(&huawei_batt_info.rpc_lock);
	rc = huawei_rpc_set_delta(delta);
	mutex_unlock(&huawei_batt_info.rpc_lock);
	if (rc < 0)
		return rc;
	return count;
}

static struct device_attribute huawei_set_delta_attrs[] = {
	__ATTR(delta, S_IWUSR | S_IWGRP, NULL, huawei_battery_set_delta),
};

static int huawei_battery_create_attrs(struct device * dev)
{
    int i, j, rc;

    for (i = 0; i < ARRAY_SIZE(huawei_battery_attrs); i++)
    {
        rc = device_create_file(dev, &huawei_battery_attrs[i]);
        BATT("huawei_battery_attrs = %d\n",rc);
        if (rc)
        {
            goto huawei_attrs_failed;
        }
    }
    
    for (j = 0; j < ARRAY_SIZE(huawei_set_delta_attrs); j++) {
		rc = device_create_file(dev, &huawei_set_delta_attrs[j]);
        BATT("huawei_set_delta_attrs = %d\n",rc);
        if (rc)
			goto huawei_delta_attrs_failed;
	}
	
	goto succeed;
	
huawei_attrs_failed:
	while (i--)
		device_remove_file(dev, &huawei_battery_attrs[i]);
huawei_delta_attrs_failed:
	while (j--)
		device_remove_file(dev, &huawei_set_delta_attrs[j]);
succeed:	
	return rc;
}

static ssize_t huawei_battery_show_property(struct device *          dev,
                                            struct device_attribute *attr,
                                            char *                   buf)
{
    int i = 0;
    const long off = attr - huawei_battery_attrs;
    
    mutex_lock(&huawei_batt_info.lock);
    switch (off)
    {
        case BATT_ID:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", huawei_batt_info.rep.batt_id);
            break;
        case POWER_OFF_CHARGING:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", huawei_batt_info.rep.power_off_charging);
            break;
        case BATT_LEVEL:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", huawei_batt_info.rep.level);
            break;
        case BATT_VOL:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", huawei_batt_info.rep.batt_vol);
            break;
        case BATT_TEMP:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", huawei_batt_info.rep.batt_temp);
            break;
        case BATT_CURRENT:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", huawei_batt_info.rep.batt_current);
            break;
        case CHARGING_SOURCE:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", huawei_batt_info.rep.charging_source);
            break;
        case CHARGING_ENABLED:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", huawei_batt_info.rep.charging_enabled);
            break;
        case FULL_BAT:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", huawei_batt_info.rep.full_bat);
            break;
        case CHARGING_EVENT:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", huawei_batt_info.rep.charging_event);
            break;
        case BATT_PRESENT:
            i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", huawei_batt_info.rep.batt_present);
            break;
        default:
            i = -EINVAL;
    }

    mutex_unlock(&huawei_batt_info.lock);
    return i;
}

static int  huawei_battery_suspend(struct platform_device* pdev, pm_message_t mesg)
{
    int rc;
    mutex_lock(&huawei_batt_info.rpc_lock);
    rc = huawei_rpc_set_delta(SUSPEND_DELTA_LEVEL);   
	mutex_unlock(&huawei_batt_info.rpc_lock);
    if (rc < 0) 
    {
        printk(KERN_ERR "%s(): set delta failed rc=%d\n", __func__, rc);
	}
    return 0;
}

static int  huawei_battery_resume(struct platform_device *pdev)
{
 
    int rc;
    mutex_lock(&huawei_batt_info.rpc_lock);
    rc = huawei_rpc_set_delta(RESUME_DELTA_LEVEL);   
	mutex_unlock(&huawei_batt_info.rpc_lock);
    if (rc < 0) 
    {
	    printk(KERN_ERR "%s(): set delta failed rc=%d\n", __func__, rc);
	}
    return 0;
}

static int huawei_battery_probe(struct platform_device *pdev)
{
    int i, rc = 0;
	
    /* init rpc */
	endpoint = msm_rpc_connect(APP_BATT_PROG, APP_BATT_VER, 0);
	if (IS_ERR(endpoint)) {
		printk(KERN_ERR "%s: init rpc failed! rc = %ld\n",
		       __FUNCTION__, PTR_ERR(endpoint));
		return rc;
	}

    /* init power supplier framework */
    for (i = 0; i < ARRAY_SIZE(huawei_power_supplies); i++)
    {
        rc = power_supply_register(&pdev->dev, &huawei_power_supplies[i]);
        if (rc)
        {
            printk(KERN_ERR "Failed to register power supply (%d)\n", rc);
        }
    }

    /* create huawei detail attributes */
    huawei_battery_create_attrs(huawei_power_supplies[CHARGER_BATTERY].dev);
    /* After battery driver gets initialized, send rpc request to inquiry
	 * the battery status in case of we lost some info
	 */
	huawei_battery_initial = 1;

	mutex_lock(&huawei_batt_info.rpc_lock);
	if (huawei_get_power_down_charging_flag(&huawei_batt_info.rep.power_off_charging) < 0)
       	printk(KERN_ERR "%s: get info failed\n", __FUNCTION__);
	
		
	if (huawei_rpc_set_delta(1) < 0)
		printk(KERN_ERR "%s: set delta failed\n", __FUNCTION__);

	mutex_unlock(&huawei_batt_info.rpc_lock);
    

    return 0;
}

/* batt_mtoa server definitions */
#define BATT_MTOA_PROG				0x30000094
#define BATT_MTOA_VERS				0x00000011
#define RPC_BATT_MTOA_NULL			0
#define RPC_BATT_MTOA_CABLE_STATUS_UPDATE_PROC	4
#define RPC_BATT_MTOA_LEVEL_UPDATE_PROC		5
/* the subcommand is unprobe the usb device when switch 
  the mobile to usb download mode by diag. 
*/
#define RPC_UNPROBE_USB_COMPOSITION_PROC   6
extern void unprobe_usb_composition(void);

struct rpc_batt_mtoa_cable_status_update_args {
	int status;
	uint32_t present;
};

struct rpc_dem_battery_update_args {
	uint32_t level;
    int temperature;
    uint32_t voltage;

};

static int handle_battery_call(struct msm_rpc_server *server,
			       struct rpc_request_hdr *req, unsigned len)
{	
   int charging_status = 0;
   int charger = 0;
    
	switch (req->procedure) {
	case RPC_BATT_MTOA_NULL:
		return 0;
    
	case RPC_BATT_MTOA_CABLE_STATUS_UPDATE_PROC: {
		struct rpc_batt_mtoa_cable_status_update_args *args;
		args = (struct rpc_batt_mtoa_cable_status_update_args *)(req + 1);
		args->status = be32_to_cpu(args->status);
		args->present = be32_to_cpu(args->present);
		BATT("charging_status_update: status=%d,present=%d\n",args->status,args->present);
        huawei_batt_info.present = args->present;
        charging_status = get_usb_ac_charging_status(args->status);
        charger = get_power_source(args->status, args->present);
        huawei_charging_event_status_update(charging_status, charger);
		return 0;
	}
	case RPC_BATT_MTOA_LEVEL_UPDATE_PROC: {
		struct rpc_dem_battery_update_args *args;
		args = (struct rpc_dem_battery_update_args *)(req + 1);
		args->level = be32_to_cpu(args->level);
        args->temperature = be32_to_cpu(args->temperature)*10;
        args->voltage = be32_to_cpu(args->voltage);
		BATT("battery_level_update: level=%d,temprature=%d,voltage=%d\n",args->level,args->temperature,args->voltage);
		huawei_battery_status_update(args->level, args->temperature, args->voltage);

		return 0;
	}
  /* the subcommand is call by function toolsdiag_dload_jump in arm9 toolsdiag.c. */
  case RPC_UNPROBE_USB_COMPOSITION_PROC:{
    unprobe_usb_composition();
    return 0;
  }
	default:
		printk(KERN_ERR "%s: program 0x%08x:%d: unknown procedure %d\n",
		       __FUNCTION__, req->prog, req->vers, req->procedure);
		return -ENODEV;
	}
}


static struct platform_driver huawei_battery_driver =
{
    .probe     = huawei_battery_probe,
    .suspend	= huawei_battery_suspend,
    .resume		= huawei_battery_resume,
    .driver    = {
        .name  = "huawei_battery",
        .owner = THIS_MODULE,
    },
};

static struct msm_rpc_server battery_server = {
	.prog = BATT_MTOA_PROG,
	.vers = BATT_MTOA_VERS,
	.rpc_call = handle_battery_call,
};
static int __devinit huawei_battery_init(void)
{
    wake_lock_init(&low_level_wake_lock, WAKE_LOCK_SUSPEND, "power_off_level_inform");
    mutex_init(&huawei_batt_info.lock);
	mutex_init(&huawei_batt_info.rpc_lock);
    msm_rpc_create_server(&battery_server);
    platform_driver_register(&huawei_battery_driver);
    return 0;
}

module_init(huawei_battery_init);
MODULE_DESCRIPTION("Huawei Battery Driver");
MODULE_LICENSE("GPL");
#endif/*CONFIG_HUAWEI_U8220_BATTERY*/

