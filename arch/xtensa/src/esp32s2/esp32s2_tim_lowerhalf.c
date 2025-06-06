/****************************************************************************
 * arch/xtensa/src/esp32s2/esp32s2_tim_lowerhalf.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>
#include <stdio.h>
#include <stdint.h>

#include <nuttx/arch.h>
#include <nuttx/timers/timer.h>

#include "hardware/esp32s2_soc.h"

#include "esp32s2_tim.h"
#include "esp32s2_clockconfig.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct esp32s2_timer_lowerhalf_s
{
  const struct timer_ops_s *ops;        /* Lower half operations */
  struct esp32s2_tim_dev_s *tim;        /* esp32s2 timer driver */
  tccb_t                    callback;   /* Interrupt callback */
  void                     *arg;        /* Argument passed to upper half callback */
  bool                      started;    /* True: Timer has been started */
  void                     *upper;      /* Pointer to watchdog_upperhalf_s */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int esp32s2_timer_handler(int irq, void *context, void *arg);

/* "Lower half" driver methods **********************************************/

static int esp32s2_timer_start(struct timer_lowerhalf_s *lower);
static int esp32s2_timer_stop(struct timer_lowerhalf_s *lower);
static int esp32s2_timer_getstatus(struct timer_lowerhalf_s *lower,
                                   struct timer_status_s *status);
static int esp32s2_timer_settimeout(struct timer_lowerhalf_s *lower,
                                    uint32_t timeout);
static int esp32s2_timer_maxtimeout(struct timer_lowerhalf_s *lower,
                                    uint32_t *timeout);
static void esp32s2_timer_setcallback(struct timer_lowerhalf_s *lower,
                                      tccb_t callback, void *arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* "Lower half" driver methods */

static const struct timer_ops_s g_esp32s2_timer_ops =
{
  .start       = esp32s2_timer_start,
  .stop        = esp32s2_timer_stop,
  .getstatus   = esp32s2_timer_getstatus,
  .settimeout  = esp32s2_timer_settimeout,
  .setcallback = esp32s2_timer_setcallback,
  .ioctl       = NULL,
  .maxtimeout  = esp32s2_timer_maxtimeout
};

#ifdef CONFIG_ESP32S2_TIMER0

/* TIMER0 lower-half */

static struct esp32s2_timer_lowerhalf_s g_esp32s2_timer0_lowerhalf =
{
  .ops = &g_esp32s2_timer_ops,
};
#endif

#ifdef CONFIG_ESP32S2_TIMER1

/* TIMER1 lower-half */

static struct esp32s2_timer_lowerhalf_s g_esp32s2_timer1_lowerhalf =
{
  .ops = &g_esp32s2_timer_ops,
};
#endif

#ifdef CONFIG_ESP32S2_TIMER2

/* TIMER2 lower-half */

static struct esp32s2_timer_lowerhalf_s g_esp32s2_timer2_lowerhalf =
{
  .ops = &g_esp32s2_timer_ops,
};
#endif

#ifdef CONFIG_ESP32S2_TIMER3

/* TIMER3 lower-half */

static struct esp32s2_timer_lowerhalf_s g_esp32s2_timer3_lowerhalf =
{
  .ops = &g_esp32s2_timer_ops,
};
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32s2_timer_handler
 *
 * Description:
 *   Timer interrupt handler
 *
 ****************************************************************************/

static int esp32s2_timer_handler(int irq, void *context, void *arg)
{
  struct esp32s2_timer_lowerhalf_s *priv =
    (struct esp32s2_timer_lowerhalf_s *)arg;
  uint32_t next_interval_us = 0;

  ESP32S2_TIM_ACKINT(priv->tim);        /* Clear the Interrupt */

  if (priv->callback(&next_interval_us, priv->upper))
    {
      if (next_interval_us > 0)
        {
          /* Set a value to the alarm */

          ESP32S2_TIM_SETALRVL(priv->tim, next_interval_us);
        }
    }
  else
    {
      esp32s2_timer_stop((struct timer_lowerhalf_s *)priv);
    }

  ESP32S2_TIM_SETALRM(priv->tim, true); /* Re-enables the alarm */
  return OK;
}

/****************************************************************************
 * Name: esp32s2_timer_start
 *
 * Description:
 *   Start the timer, resetting the time to the current timeout
 *
 * Input Parameters:
 *   lower - A pointer to the representation of
 *           the "lower-half" driver state structure.
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int esp32s2_timer_start(struct timer_lowerhalf_s *lower)
{
  struct esp32s2_timer_lowerhalf_s *priv =
    (struct esp32s2_timer_lowerhalf_s *)lower;
  int ret = OK;
  uint16_t pre;
  irqstate_t flags;

  DEBUGASSERT(priv);

  if (priv->started == true)
    {
      /* Return EBUSY to indicate that the timer is already running */

      ret = -EBUSY;
      goto errout;
    }

  /* Make sure the timer is stopped to avoid unpredictable behavior */

  ESP32S2_TIM_STOP(priv->tim);

  /* Configure clock source */

  ESP32S2_TIM_CLK_SRC(priv->tim, ESP32S2_TIM_APB_CLK);

  /* Calculate the suitable prescaler according to the current APB
   * frequency to generate a period of 1 us.
   */

  pre = esp_clk_apb_freq() / 1000000;

  /* Configure TIMER prescaler */

  ESP32S2_TIM_SETPRE(priv->tim, pre);

  /* Configure TIMER mode */

  ESP32S2_TIM_SETMODE(priv->tim, ESP32S2_TIM_MODE_UP);

  /* Clear TIMER counter value */

  ESP32S2_TIM_CLEAR(priv->tim);

  /* Enable autoreload */

  ESP32S2_TIM_SETARLD(priv->tim, true);

  /* Enable TIMER alarm */

  ESP32S2_TIM_SETALRM(priv->tim, true);

  /* Clear Interrupt Bits Status */

  ESP32S2_TIM_ACKINT(priv->tim);

  /* Configure callback, in case a handler was provided before */

  if (priv->callback != NULL)
    {
      flags = enter_critical_section();
      ret = ESP32S2_TIM_SETISR(priv->tim, esp32s2_timer_handler, priv);
      leave_critical_section(flags);
      if (ret != OK)
        {
          goto errout;
        }

      ESP32S2_TIM_ENABLEINT(priv->tim);
    }

  /* Finally, start the TIMER */

  ESP32S2_TIM_START(priv->tim);
  priv->started = true;

errout:
  return ret;
}

/****************************************************************************
 * Name: esp32s2_timer_stop
 *
 * Description:
 *   Stop the timer
 *
 * Input Parameters:
 *   lower - A pointer the publicly visible representation of
 *           the "lower-half" driver state structure.
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int esp32s2_timer_stop(struct timer_lowerhalf_s *lower)
{
  struct esp32s2_timer_lowerhalf_s *priv =
    (struct esp32s2_timer_lowerhalf_s *)lower;
  int ret = OK;
  irqstate_t flags;

  DEBUGASSERT(priv);

  if (priv->started == false)
    {
      /* Return ENODEV to indicate that the timer was not running */

      ret = -ENODEV;
      goto errout;
    }

  if (priv->callback != NULL)
    {
      flags = enter_critical_section();
      ESP32S2_TIM_DISABLEINT(priv->tim);
      ret = ESP32S2_TIM_SETISR(priv->tim, NULL, NULL);
      leave_critical_section(flags);
      priv->callback = NULL;
    }

  ESP32S2_TIM_STOP(priv->tim);
  priv->started = false;

errout:
  return ret;
}

/****************************************************************************
 * Name: esp32s2_timer_getstatus
 *
 * Description:
 *   Get timer status.
 *
 * Input Parameters:
 *   lower  - A pointer the publicly visible representation of the "lower-
 *            half" driver state structure.
 *   status - The location to return the status information.
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int esp32s2_timer_getstatus(struct timer_lowerhalf_s *lower,
                                   struct timer_status_s *status)
{
  struct esp32s2_timer_lowerhalf_s *priv =
    (struct esp32s2_timer_lowerhalf_s *)lower;
  int ret = OK;
  uint64_t current_counter_value;
  uint64_t alarm_value;

  DEBUGASSERT(priv);
  DEBUGASSERT(status);

  /* Return the status bit */

  status->flags = 0;

  if (priv->started == true)
    {
      /* TIMER is running */

      status->flags |= TCFLAGS_ACTIVE;
    }

  if (priv->callback != NULL)
    {
      /* TIMER has a user callback function to be called when
       * expiration happens
       */

      status->flags |= TCFLAGS_HANDLER;
    }

  /* Get the current counter value */

  ESP32S2_TIM_GETCTR(priv->tim, &current_counter_value);

  /* Get the current configured timeout */

  ESP32S2_TIM_GETALRVL(priv->tim, &alarm_value);

  status->timeout  = (uint32_t)(alarm_value);
  status->timeleft = (uint32_t)(alarm_value - current_counter_value);

  return ret;
}

/****************************************************************************
 * Name: esp32s2_timer_settimeout
 *
 * Description:
 *   Set a new timeout value.
 *
 * Input Parameters:
 *   lower   - A pointer the publicly visible representation of
 *             the "lower-half" driver state structure.
 *   timeout - The new timeout value in microseconds.
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int esp32s2_timer_settimeout(struct timer_lowerhalf_s *lower,
                                    uint32_t timeout)
{
  struct esp32s2_timer_lowerhalf_s *priv =
    (struct esp32s2_timer_lowerhalf_s *)lower;
  int ret = OK;

  DEBUGASSERT(priv);

  /* Set the timeout */

  ESP32S2_TIM_SETALRVL(priv->tim, (uint64_t)timeout);

  return ret;
}

/****************************************************************************
 * Name: esp32s2_timer_maxtimeout
 *
 * Description:
 *   Get the maximum timeout value
 *
 * Input Parameters:
 *   lower       - A pointer the publicly visible representation of
 *                 the "lower-half" driver state structure.
 *   maxtimeout  - A pointer to the variable that will store the max timeout.
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int esp32s2_timer_maxtimeout(struct timer_lowerhalf_s *lower,
                                  uint32_t *max_timeout)
{
  DEBUGASSERT(max_timeout);

  *max_timeout = UINT32_MAX;

  return OK;
}

/****************************************************************************
 * Name: esp32s2_setcallback
 *
 * Description:
 *   Set the provided callback to be called at timeout from within the
 *   ISR.
 *
 * Input Parameters:
 *   lower    - A pointer to the publicly visible representation of
 *              the "lower-half" driver state structure.
 *   callback - A pointer to the callback. If this function pointer
 *              is NULL, then the reset-on-expiration behavior is restored.
 *   arg      - A pointer to the argument that will be provided to
 *              the callback
 *
 ****************************************************************************/

static void esp32s2_timer_setcallback(struct timer_lowerhalf_s *lower,
                                      tccb_t callback, void *arg)
{
  struct esp32s2_timer_lowerhalf_s *priv =
    (struct esp32s2_timer_lowerhalf_s *)lower;
  irqstate_t flags;
  int ret = OK;

  DEBUGASSERT(priv);

  /* Save the new callback */

  priv->callback = callback;
  priv->arg      = arg;

  flags = enter_critical_section();

  /* There is a user callback and the timer has already been started */

  if (callback != NULL && priv->started)
    {
      ret = ESP32S2_TIM_SETISR(priv->tim, esp32s2_timer_handler, priv);
      ESP32S2_TIM_ENABLEINT(priv->tim);
    }
  else
    {
      ESP32S2_TIM_DISABLEINT(priv->tim);
      ret = ESP32S2_TIM_SETISR(priv->tim, NULL, NULL);
    }

  leave_critical_section(flags);
  if (ret != OK)
    {
      tmrerr("Error to set ISR: %d", ret);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32s2_timer_initialize
 *
 * Description:
 *   Bind the configuration timer to a timer lower half instance and
 *   register the timer drivers at 'devpath'
 *
 * Input Parameters:
 *   devpath - The full path to the timer device.  This should be of the
 *             form /dev/timerx.
 *   timer - the timer's number.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; A negated errno value is returned
 *   to indicate the nature of any failure.
 *
 ****************************************************************************/

int esp32s2_timer_initialize(const char *devpath, uint8_t timer)
{
  struct esp32s2_timer_lowerhalf_s *lower = NULL;
  int ret = OK;

  DEBUGASSERT(devpath);

  switch (timer)
    {
#ifdef CONFIG_ESP32S2_TIMER0
      case TIMER0:
        {
          lower = &g_esp32s2_timer0_lowerhalf;
          break;
        }
#endif

#ifdef CONFIG_ESP32S2_TIMER1
      case TIMER1:
        {
          lower = &g_esp32s2_timer1_lowerhalf;
          break;
        }
#endif

#ifdef CONFIG_ESP32S2_TIMER2
      case TIMER2:
        {
          lower = &g_esp32s2_timer2_lowerhalf;
          break;
        }
#endif

#ifdef CONFIG_ESP32S2_TIMER3
      case TIMER3:
        {
          lower = &g_esp32s2_timer3_lowerhalf;
          break;
        }
#endif

      default:
        {
          ret = -ENODEV;
          goto errout;
        }
    }

  /* Initialize the elements of lower half state structure */

  lower->started  = false;
  lower->callback = NULL;
  lower->tim      = esp32s2_tim_init(timer);

  if (lower->tim == NULL)
    {
      ret = -EINVAL;
      goto errout;
    }

  /* Register the timer driver as /dev/timerX.  The returned value from
   * timer_register is a handle that could be used with timer_unregister().
   * REVISIT: The returned handle is discarded here.
   */

  lower->upper = timer_register(devpath,
                                (struct timer_lowerhalf_s *)lower);
  if (lower->upper == NULL)
    {
      /* The actual cause of the failure may have been a failure to allocate
       * perhaps a failure to register the timer driver (such as if the
       * 'devpath' were not unique).  We know here but we return EEXIST to
       * indicate the failure (implying the non-unique devpath).
       */

      ret = -EEXIST;
      goto errout;
    }

errout:
  return ret;
}
