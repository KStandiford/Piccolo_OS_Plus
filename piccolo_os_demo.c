/*
 * Copyright (C) 2021 Gary Sims
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include <stdio.h>
#include "piccolo_os.h"

const uint LED_PIN = 25;
const uint LED2_PIN = 14;

void task1_func(void) {
  piccolo_sleep_t t;
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  while (true) {
    gpio_put(LED_PIN, 1);
    piccolo_sleep(&t, 1000);
    gpio_put(LED_PIN, 0);
    piccolo_sleep(&t, 1000);
  }
}
#define notprimes
#ifndef notprimes
int is_prime(unsigned int n)
{
	unsigned int p;
	if (!(n & 1) || n < 2 ) return n == 2;
 
	// comparing p*p <= n can overflow 
	for (p = 3; p <= n/p; p += 2)
		if (!(n % p)) return 0;
	return 1;
}

//  Prime number computing task

void task2_func(void) {
  piccolo_sleep_t t;
  int p;

  printf("task2: Created!\n");
  while (1) {
//    p = to_ms_since_boot(get_absolute_time());
    p++;
    if(is_prime(p)==1) {
      printf("%d is prime!\n", p);
    }
    piccolo_sleep(&t,100);
  }
}
#else

//  test timers from SDK

/// \tag::timer_example[]
volatile bool timer_fired = false;

int64_t alarm_callback(alarm_id_t id, void *user_data) {
    printf("Timer %d fired!\n", (int) id);
    timer_fired = true;
    // Can return a value here in us to fire in the future
    return 0;
}

bool repeating_timer_callback(struct repeating_timer *t) {
    printf("Repeat at %lld\n", time_us_64());
    return true;
}


void task2_func(void) {
    piccolo_sleep_t t;
    bool cancelled;

    while(1) {
        timer_fired = false;
        printf("Hello Timer!\n");

        // Call alarm_callback in 2 seconds
        add_alarm_in_ms(2000, alarm_callback, NULL, false);

        // Wait for alarm callback to set timer_fired
        while (!timer_fired) {
            piccolo_yield();
        }

        // Create a repeating timer that calls repeating_timer_callback.
        // If the delay is > 0 then this is the delay between the previous callback ending and the next starting.
        // If the delay is negative (see below) then the next call to the callback will be exactly 500ms after the
        // start of the call to the last callback
        struct repeating_timer timer;
        add_repeating_timer_ms(500, repeating_timer_callback, NULL, &timer);
        piccolo_sleep(&t, 3000);
        bool cancelled = cancel_repeating_timer(&timer);
        printf("cancelled... %d\n", cancelled);
        piccolo_sleep(&t, 2000);

        // Negative delay so means we will call repeating_timer_callback, and call it again
        // 500ms later regardless of how long the callback took to execute
        add_repeating_timer_ms(-500, repeating_timer_callback, NULL, &timer);
        piccolo_sleep(&t, 3000);
        cancelled = cancel_repeating_timer(&timer);
        printf("cancelled... %d\n", cancelled);
        piccolo_sleep(&t, 2000);
        printf("Done\n");
    }
}
#endif

void task3_func(void) {
  piccolo_sleep_t t;
  gpio_init(LED2_PIN);
  gpio_set_dir(LED2_PIN, GPIO_OUT);
  while (true) {
    gpio_put(LED2_PIN, 1);
    piccolo_sleep(&t, 75);
    gpio_put(LED2_PIN, 0);
    piccolo_sleep(&t, 75);
  }
}

int main() {
    stdio_init_all();
    printf("Hello World!");
  sleep_ms(5000);  // pause to give user a chance to start putty
  piccolo_init();

  printf("PICCOLO OS Demo Starting...\n");

  piccolo_create_task(&task1_func);
  piccolo_create_task(&task2_func);
  piccolo_create_task(&task3_func);

  piccolo_start();

  return 0; /* Never gonna happen */
}
