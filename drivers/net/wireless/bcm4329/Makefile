# bcm4329
DHDCFLAGS = -DLINUX -DBCMDRIVER -DBCMDONGLEHOST -DDHDTHREAD               \
	-DBCMWPA2 -DBCMWAPI_WPI -DUNRELEASEDCHIP -DCONFIG_WIRELESS_EXT        \
	-DDHD_GPL -DDHD_SCHED -DBDC -DTOE -DDHD_BCMEVENTS                     \
	-DSHOW_EVENTS -DBCMSDIO -DDHD_GPL -DHW_OOB -DOOB_INTR_ONLY            \
	-DBCMLXSDMMC -DBCMPLATFORM_BUS -DEMBEDDED_PLATFORM -DOEM_ANDROID      \
	-DARP_OFFLOAD_SUPPORT -DPKT_FILTER_SUPPORT                            \
	-DCUSTOMER_HW2 -DCUSTOM_OOB_GPIO_NUM=17 -DCUSTOM_SDCC_WIFI_SLOT=2     \
	-DSOFTAP -Wall -Wstrict-prototypes                                    \
	-Idrivers/net/wireless/bcm4329 -Idrivers/net/wireless/bcm4329/include

DHDOFILES = dhd_linux.o linux_osl.o bcmutils.o dhd_common.o dhd_custom_gpio.o \
	siutils.o sbutils.o aiutils.o hndpmu.o dhd_linux_sched.o bcmwifi.o    \
	wl_iw.o dhd_cdc.o dhd_sdio.o bcmsdh_sdmmc.o bcmsdh.o bcmsdh_linux.o   \
	bcmsdh_sdmmc_linux.o

obj-$(CONFIG_BCM4329) += dhd.o
dhd-objs += $(DHDOFILES)
EXTRA_CFLAGS = $(DHDCFLAGS)
EXTRA_LDFLAGS += --strip-debug
