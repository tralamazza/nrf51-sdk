DEVICE_VARIANT?= xxaa

DEFINES+= NRF51 NRF51822_QFAA_CA
SDKINCDIRS+= . gcc sd_common app_common

SDKSRCS+= templates/gcc/gcc_startup_nrf51.s templates/system_nrf51.c
USE_SOFTDEVICE?= s110
SOFTDEV_HEX?= $(lastword $(wildcard ${USE_SOFTDEVICE}_nrf51822_*_softdevice.hex))

SDKDIR?= $(abspath $(dir $(lastword ${MAKEFILE_LIST})))
ifndef SDDIR
SDKINCDIRS+= ${USE_SOFTDEVICE}
else
CFLAGS+= -I${SDDIR}/include
endif


CPPFLAGS+= $(patsubst %,-D%,${DEFINES})

CFLAGS+= -I${SDKDIR}/relayr/include
CFLAGS+= $(patsubst %,-I${SDKDIR}/nrf51822/Include/%,${SDKINCDIRS})
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
LDFLAGS+= -Wl,-L${SDKDIR}/nrf51822/Source/templates/gcc
LDFLAGS+= -Wl,-T,${LINKERSCRIPT}


ASFLAGS+= -x assembler-with-cpp


SRCS+=	$(patsubst %,${SDKDIR}/nrf51822/Source/%,${SDKSRCS})


CC=	arm-none-eabi-gcc
OBJCOPY=	arm-none-eabi-objcopy


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
	arm-none-eabi-objdump -h $< | \
	awk '$$1 ~ /^[0-9]+$$/ {addr="0x"$$5; if (!min || addr < min) min = addr} END { printf "\
	loadbin %s,%s\n\
	r\n\
	g\n\
	exit\n", f, min}' f="$<" > $@

%-all.jlink: %.jlink ${SOFTDEV_HEX}
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

clean:
	-rm -f ${OBJS} ${OBJS:.o=.d} ${PROG}.hex ${PROG}.elf ${PROG}.map ${PROG}.jlink
