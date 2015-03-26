#include <stdlib.h>

#include "simble.h"
#include "rtc.h"


// Configure the Tick interval, 0x20 = 32.768/32 = 1.024
#define RTC_PRESCALER 31u


static struct rtc_ctx *ctx;

/* RTC : the default TICK_INTERVAL is 1ms, the module can manage up to 4 compare
      registers */
void
rtc_update_cfg(uint32_t value, uint8_t timer_id, bool enabled)
{
  ctx->rtc_x[timer_id].period = value;
  //if period = 0 ->disable timer
  ctx->rtc_x[timer_id].enabled = (enabled & (bool)value);

  if (enabled && (ctx->used_timers <= RTC_MAX_TIMERS)){
    NRF_RTC1->EVENTS_COMPARE[timer_id] = 0;
    NRF_RTC1->CC[timer_id] = NRF_RTC1->COUNTER + ctx->rtc_x[timer_id].period;
  }
  cfg_int_mask(timer_id, ctx->rtc_x[timer_id].enabled);
}

void
cfg_int_mask(uint8_t timer_id, bool enabled)
{
  if (enabled)
    NRF_RTC1->INTENSET = RTC_INTENSET_COMPARE0_Msk << timer_id;
  else
    NRF_RTC1->INTENCLR = RTC_INTENCLR_COMPARE0_Msk << timer_id;
}

void
rtc_init(struct rtc_ctx *c)
{
  ctx = c;

  sd_nvic_ClearPendingIRQ(RTC1_IRQn);
  sd_nvic_SetPriority(RTC1_IRQn, NRF_APP_PRIORITY_LOW);
  sd_nvic_EnableIRQ(RTC1_IRQn);

  //The LFCLK runs at 32.768 Hz, divided by (PRESCALER+1) = 1.024 ms
  NRF_RTC1->PRESCALER = RTC_PRESCALER;
  // Disable the Event routing to the PPI to save power
  NRF_RTC1->EVTEN = 0;

  for (int id=0; id < RTC_MAX_TIMERS; id++) {
    if ((ctx->used_timers <= RTC_MAX_TIMERS) && (ctx->rtc_x[id].period != 0))
    {
      // Config. CC[x] module to generate interrupts and events
      NRF_RTC1->CC[id] += ctx->rtc_x[id].period;
      NRF_RTC1->EVTENSET = RTC_EVTENSET_COMPARE0_Msk << id;
      cfg_int_mask(id, ctx->rtc_x[id].enabled);
      ctx->used_timers++;
    }
  }
  // Reset the Counter
  NRF_RTC1->TASKS_CLEAR = 1;
  // Start the RTC1
  NRF_RTC1->TASKS_START = 1;
}

bool
rtc_oneshot_timer(uint32_t value, rtc_evt_cb_t *cb)
{
  int timer_id;
  //Setup cb and assign a timer_id if free, return false is all used
  if (ctx->used_timers <= RTC_MAX_TIMERS && (value !=0)){
    timer_id = ctx->used_timers;

    ctx->used_timers++;
    ctx->rtc_x[timer_id].type = ONE_SHOT;
    ctx->rtc_x[timer_id].cb = cb;
    //ctx->rtc_x[timer_id].enabled = true;

    rtc_update_cfg(value, timer_id, true);
    return true;
  }
  else return false;
}

/* The RTC1 instance IRQ handler*/
void
RTC1_IRQHandler(void)
{
  //NRF_GPIO->OUT ^= (1 << 1);
  for (uint8_t id = 0; id < ctx->used_timers; id++){
    if (NRF_RTC1->EVENTS_COMPARE[id] != 0)
    {
        // prepare the comparator for the next interval
        NRF_RTC1->CC[id] += ctx->rtc_x[id].period;

        // clear the event CC_x
        NRF_RTC1->EVENTS_COMPARE[id] = 0;

        //Toggle pin2 for test
        NRF_GPIO->OUT ^= (1 << 2);

        if (ctx->rtc_x[id].type == ONE_SHOT){
          //release the timer
          ctx->rtc_x[id].enabled = false;
          ctx->used_timers--;
          cfg_int_mask(id, false);
        }
        //call the registered callback
        ctx->rtc_x[id].cb(ctx);
    }
  }
}
