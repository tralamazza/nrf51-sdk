RELAYR_ROOT?= ${SDKDIR}/relayr

CFLAGS+= \
	-I${RELAYR_ROOT}/src

SDKINCDIRS+= \
	device \
	ble/common \
	libraries/util

SRCS+= \
	${RELAYR_ROOT}/src/i2c.c \
	${RELAYR_ROOT}/src/indicator.c \
	${RELAYR_ROOT}/src/onboard-led.c \
	${RELAYR_ROOT}/src/simble.c \
	${RELAYR_ROOT}/src/util.c \
	${RELAYR_ROOT}/src/batt_serv.c \
	${RELAYR_ROOT}/src/rtc.c

ifeq (${DEBUG_LOG},rtt)
CFLAGS+= \
	-I${SDKDIR}/segger/RTT

SRCS+= \
	${RELAYR_ROOT}/src/segger_rtt_init.c \
	${SDKDIR}/segger/RTT/SEGGER_RTT.c \
	${SDKDIR}/segger/RTT/SEGGER_RTT_printf.c \
	${SDKDIR}/segger/Syscalls/RTT_Syscalls_GCC.c

DEFINES+= ENABLE_RTT_DEBUG_LOG
endif

SDKSRCS+= \
	drivers_nrf/pstorage/pstorage.c

ifeq (${USE_SOFTDEVICE},s120)
DEFINES+= SD120

SRCS+= \
	${RELAYR_ROOT}/src/simble_central.c

SDKSRCS+= \
	ble/ble_db_discovery/ble_db_discovery.c \
	ble/device_manager/device_manager_central.c
endif

DEFINES+= BLE_STACK_SUPPORT_REQD SOFTDEVICE_PRESENT __HEAP_SIZE=0
