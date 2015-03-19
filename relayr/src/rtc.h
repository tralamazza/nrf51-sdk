#ifndef RTC_H
#define RTC_H


/**< Number of compare channels in the RTC1 instance. */
#define RTC_CHANNEL_NUM 4
/**< Input frequency of the RTC instance. */
#define RTC_INPUT_FREQ 32768


void rtc_init(void);

#endif
