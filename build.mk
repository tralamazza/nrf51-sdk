DEVICE_VARIANT?= xxaa

DEFINES+= NRF51 NRF51822_QFAA_CA
SDKINCDIRS+= . gcc sd_common app_common

SDKSRCS+= templates/gcc/gcc_startup_nrf51.s templates/system_nrf51.c
USE_SOFTDEVICE?= s110

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

#LDFLAGS+= -nodefaultlibs
LDFLAGS+= -Wl,--gc-sections -fwhole-program
LDFLAGS+= -Wl,-Map=${PROG}.map
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
clean:
	-rm -f ${OBJS} ${OBJS:.o=.d} ${PROG}.hex ${PROG}.elf ${PROG}.map
