DEVICE_VARIANT?= xxaa

DEFINES+= NRF51 NRF51822_QFAA_CA
SDKINCDIRS+= toolchain toolchain/gcc drivers_nrf/hal

SDKSRCS+= toolchain/gcc/gcc_startup_nrf51.s toolchain/system_nrf51.c
USE_SOFTDEVICE?= s110
SOFTDEV_HEX?= $(lastword $(wildcard ${USE_SOFTDEVICE}_nrf51*_softdevice.hex))

SDKDIR?= $(abspath $(dir $(lastword ${MAKEFILE_LIST})))
ifndef SDDIR
SDKINCDIRS+= softdevice/${USE_SOFTDEVICE}/headers
else
CFLAGS+= -I${SDDIR}/include
endif

SDKINCDIRS+= $(sort $(dir ${SDKSRCS}) )

ifdef USE_RTX
DEFINES+= RTX __HEAP_SIZE=0 __STACK_SIZE=1024
SRCS+= ${SDKDIR}/external/rtx/port/RTX_Conf_CM.c 
CFLAGS+= -I${SDKDIR}/external/rtx/include
LDLIBS+= ${SDKDIR}/external/rtx/source/GCC/libRTX_CM0.a
endif

CPPFLAGS+= $(patsubst %,-D%,${DEFINES})

CFLAGS+= -I${SDKDIR}/relayr/include
CFLAGS+= $(patsubst %,-I${SDKDIR}/components/%,${SDKINCDIRS})
CFLAGS+= -mcpu=cortex-m0 -mfloat-abi=soft -mthumb -mabi=aapcs	\
	-ffunction-sections -fdata-sections -fno-builtin \
	-fplan9-extensions
CFLAGS+= -std=gnu11
CFLAGS+= -Wall -Wno-main
CFLAGS+= -g

LINKERSCRIPT?= gcc_nrf51_${USE_SOFTDEVICE}_${DEVICE_VARIANT}.ld

LDFLAGS+= -Wl,--gc-sections -fwhole-program --specs=nano.specs
LDFLAGS+= -Wl,-Map=${PROG}.map
LDFLAGS+= -Wl,-T,${SDKDIR}/relayr/ld/libc-nano.ld
LDFLAGS+= -Wl,-L${SDKDIR}/relayr/ld
LDFLAGS+= -Wl,-L${SDKDIR}/components/toolchain/gcc
LDFLAGS+= -Wl,-T,${LINKERSCRIPT}


ASFLAGS+= -x assembler-with-cpp


SRCS+=	$(patsubst %,${SDKDIR}/components/%,${SDKSRCS})


CC=	arm-none-eabi-gcc
OBJCOPY=	arm-none-eabi-objcopy
OBJDUMP=	arm-none-eabi-objdump
GDB=	arm-none-eabi-gdb

GENERATE.d=	$(CC) -MM ${CFLAGS} ${CPPFLAGS} -MT $@ -MT ${@:.d=.o} -MP -MF $@ $<
COMPILE.s=	${COMPILE.S}


all: ${PROG}.hex


define compile_source
$(addsuffix .o,$(notdir $(basename ${1}))): ${1}
	$${COMPILE$(suffix ${1})} $${OUTPUT_OPTION} $$<

ifneq ($(filter .c,${1}),)
$(addsuffix .d,$(notdir $(basename ${1}))): ${1}
	$$(GENERATE.d)
endif

OBJS+=	$$(addsuffix .o,$(notdir $(basename ${1})))
endef

$(foreach f,${SRCS},$(eval $(call compile_source,$f)))

ifneq (${MAKECMDGOALS},clean)
-include $(patsubst %.o,%.d,${OBJS})
endif


${PROG}.elf: ${OBJS}
	${CC} -o $@ ${CFLAGS} ${LDFLAGS} ${OBJS} ${LDLIBS}

%.hex: %.elf
	${OBJCOPY} -O ihex $< $@

%.jlink: %.hex
	${OBJDUMP} -h $< | \
	awk '$$1 ~ /^[0-9]+$$/ {addr="0x"$$5; if (!min || addr < min) min = addr} END { printf "\
	loadbin %s,%s\n\
	r\n\
	g\n\
	exit\n", f, min}' f="$<" > $@

%-all.jlink: %.jlink ${SOFTDEV_HEX} FORCE
	@[ -e "${SOFTDEV_HEX}" ] || echo "cannot find softdevice hex image '${SOFTDEV_HEX}'" >&2
	printf "\
	device NRF51822_XXAA\n\
	halt\n\
	w4 0x4001e504,2	# enable erase: CONFIG.WEN = EEN\n\
	w4 0x4001e50c,1 # erase all: ERASEALL = 1\n\
	sleep 1\n\
	loadbin %s,0\n" ${SOFTDEV_HEX} > $@
	cat $< >> $@

flash: ${PROG}.hex ${PROG}.jlink
	JLinkExe -device nRF51822_xxAA -if SWD ${PROG}.jlink

flash-all: ${PROG}.hex ${SOFTDEV_HEX} ${PROG}-all.jlink
	JLinkExe -device nRF51822_xxAA -if SWD ${PROG}-all.jlink

gdbserver: ${PROG}.elf
	JLinkGDBServer -device nRF51822_xxAA -if SWD

gdb: ${PROG}.elf
	${GDB} ${PROG}.elf -ex 'target remote :2331'

clean:
	-rm -f ${OBJS} ${OBJS:.o=.d} ${PROG}.hex ${PROG}.elf ${PROG}.map ${PROG}.jlink ${PROG}-all.jlink

.PHONY: all flash flash-all gdbserver gdb clean FORCE
