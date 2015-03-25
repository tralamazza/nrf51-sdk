#ifndef RTC_H
#define RTC_H


/**< Number of compare channels in the RTC1 instance. */
#define RTC_CHANNEL_NUM 4
/**< Input frequency of the RTC instance. */
#define RTC_INPUT_FREQ 32768

#define RTC_MAX_TIMERS  4

struct rtc_ctx;

typedef void (rtc_evt_cb_t)(struct rtc_ctx *ctx);

struct rtc_x {
  uint32_t period;  //24 bits, max value: 16777216 (~4 hours @ 1ms tick)
  uint8_t enabled;
  rtc_evt_cb_t *cb;
};

struct rtc_ctx {
	uint8_t used_timers;  //Max 4
  struct rtc_x rtc_x[RTC_MAX_TIMERS];
};

void rtc_update_cfg(uint32_t value, uint8_t timer_id);
void rtc_init(struct rtc_ctx *ctx);

#endif
