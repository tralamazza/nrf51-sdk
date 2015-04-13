#pragma once

#ifdef ENABLE_RTT_DEBUG_LOG

#include "segger_rtt_init.h"

#define app_trace_init segger_rtt_init
#define app_trace_log segger_rtt_printf
#define app_trace_dump(...)

#else

#define app_trace_init(...)
#define app_trace_log(...)
#define app_trace_dump(...)

#endif
