#include "segger_rtt_init.h"
#include <stddef.h>

void
segger_rtt_init(void)
{
#ifdef BLOCKING_LOG
	SEGGER_RTT_ConfigUpBuffer(0, NULL, NULL, 0, SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
#else
	SEGGER_RTT_ConfigUpBuffer(0, NULL, NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
#endif
}
