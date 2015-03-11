#pragma once

#include "SEGGER_RTT.h"

#define segger_rtt_printf(...) SEGGER_RTT_printf(0, __VA_ARGS__)
#define segger_rtt_writestring(...) SEGGER_RTT_WriteString(0, __VA_ARGS__)

void segger_rtt_init(void);
