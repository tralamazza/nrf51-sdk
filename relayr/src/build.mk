RELAYR_ROOT?= ${SDKDIR}/relayr

CFLAGS+= \
	-I${RELAYR_ROOT}/src \
	-I${SDKDIR}/segger/RTT

SRCS+= ${RELAYR_ROOT}/src/indicator.c \
	${RELAYR_ROOT}/src/onboard-led.c \
	${RELAYR_ROOT}/src/simble.c \
	${RELAYR_ROOT}/src/util.c \
	${RELAYR_ROOT}/src/batt_serv.c \
	${RELAYR_ROOT}/src/segger_rtt.c \
	${SDKDIR}/segger/RTT/SEGGER_RTT.c \
	${SDKDIR}/segger/RTT/SEGGER_RTT_printf.c \
	${SDKDIR}/segger/Syscalls/RTT_Syscalls_GCC.c
