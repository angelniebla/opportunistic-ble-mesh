/**
 * Copyright (c) 2015 - 2020, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 *
 * @defgroup led_softblink_example_main main.c
 * @{
 * @ingroup led_softblink_example
 * @brief LED Soft Blink Example Application main file.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "boards.h"
#include "app_error.h"
#include "sdk_errors.h"
#include "app_timer.h"
#include "nrf_log.h"


//#define LED_PWR   NRF_GPIO_PIN_MAP(1,9) 

#define CE    NRF_GPIO_PIN_MAP(1,11) 
#define DAT   NRF_GPIO_PIN_MAP(0,15) 
#define SCLK  NRF_GPIO_PIN_MAP(0,14) 

uint8_t burstArray[8];

uint8_t readByte()
{
        nrf_gpio_cfg_input(DAT, NRF_GPIO_PIN_PULLUP);

	uint8_t value = 0;
	uint8_t currentBit = 0;

	for (int i = 0; i < 8; ++i)
	{
		currentBit = nrf_gpio_pin_read(DAT); 
		value |= (currentBit << i);
		nrf_gpio_pin_write(SCLK, 1);
                nrf_delay_us(1);
		nrf_gpio_pin_write(SCLK, 0);
	}
	return value;
}

void writeByte(uint8_t value, bool readAfter)
{
  nrf_gpio_cfg_output(DAT);
  for (int i = 0; i < 8; ++i) {
    nrf_gpio_pin_write(DAT, (value >> i) & 1);
    nrf_delay_us(1);
    nrf_gpio_pin_write(SCLK, 1);
    nrf_delay_us(1);

    if (readAfter && i == 7) {
      // We're about to read data -- ensure the pin is back in input mode
      // before the clock is lowered.
      nrf_gpio_cfg_input(DAT, NRF_GPIO_PIN_PULLUP);
    } else {
      nrf_gpio_pin_write(SCLK, 0);
      nrf_delay_us(1);
    }
  }
}

void burstRead()
{
	nrf_gpio_pin_write(SCLK, 0);
	nrf_gpio_pin_write(CE, 1);

	writeByte(191, true);  // BF aka BustRead
	for (int i=0; i<8; i++)
	{
		burstArray[i] = readByte();
	}
	nrf_gpio_pin_write(CE, 0);
}

uint8_t	decode(uint8_t value)
{
	uint8_t decoded = value & 127;
	decoded = (decoded & 15) + 10 * ((decoded & (15 << 4)) >> 4);
	return decoded;
}

uint8_t decodeH(uint8_t value)
{
  if (value & 128)
    value = (value & 15) + (12 * ((value & 32) >> 5));
  else
    value = (value & 15) + (10 * ((value & 48) >> 4));
  return value;
}

uint8_t	decodeY(uint8_t value)
{
	uint8_t decoded = (value & 15) + 10 * ((value & (15 << 4)) >> 4);
	return decoded;
}

/*
Time getTime()
{
	Time t;
	burstRead();
	t.sec	= _decode(_burstArray[0]);
	t.min	= _decode(_burstArray[1]);
	t.hour	= _decodeH(_burstArray[2]);
	t.date	= _decode(_burstArray[3]);
	t.mon	= _decode(_burstArray[4]);
	t.dow	= _burstArray[5];
	t.year	= _decodeY(_burstArray[6])+2000;
	return t;
}
*/
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

int main (void)
{
    log_init();
    NRF_LOG_INFO("DS1302 example started.");

  while (1) {
    NRF_LOG_INFO("sec %d", decode(burstArray[0]));
    NRF_LOG_INFO("min %d", decode(burstArray[1]));
    NRF_LOG_INFO("hr %d", decode(burstArray[2]));
    nrf_delay_ms(1000);
    NRF_LOG_FLUSH();
  }
// Testing gpio
/*
   nrf_gpio_cfg_output(LED_PWR);
   nrf_gpio_pin_clear(LED_PWR);
   while(1)
   {
       nrf_gpio_pin_toggle(LED_PWR);
       nrf_delay_ms(1000);
   }
*/

}