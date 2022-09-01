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
 * @defgroup tw_sensor_example main.c
 * @{
 * @ingroup nrf_twi_example
 * @brief TWI Sensor Example main file.
 *
 * This file contains the source code for a sample application using TWI.
 *
 */

#include <stdio.h>
#include "boards.h"
#include "app_util_platform.h"
#include "app_error.h"
#include "nrf_drv_twi.h"
#include "nrf_delay.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

/* TWI instance ID. */
#define TWI_INSTANCE_ID     0

/* Common addresses definition for DS1302 rtc. */

#define REG_SECONDS   0x80
#define REG_MINUTES   0x82
#define REG_HOURS     0x84
#define REG_DATES     0x86 // Day of the month
#define REG_MONTHS    0x88
#define REG_DAYS      0x8A // Day of the week 
#define REG_YEARS     0x8C
#define WRITE_PROTECT 0x8E
#define CHARGE        0x90
#define CLK_BURST_WT  0xBE
#define CLK_BURST_RD  0xBF
#define RAM_BASE      0xC0

/**@brief Date and Time structure. */
typedef struct
{
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hours;
    uint8_t  minutes;
    uint8_t  seconds;
} ble_date_time_t;

static __INLINE uint8_t ble_date_time_encode(const ble_date_time_t * p_date_time,
                                             uint8_t *               p_encoded_data)
{
    uint8_t len = uint16_encode(p_date_time->year, p_encoded_data);

    p_encoded_data[len++] = p_date_time->month;
    p_encoded_data[len++] = p_date_time->day;
    p_encoded_data[len++] = p_date_time->hours;
    p_encoded_data[len++] = p_date_time->minutes;
    p_encoded_data[len++] = p_date_time->seconds;

    return len;
}

static __INLINE uint8_t ble_date_time_decode(ble_date_time_t * p_date_time,
                                             const uint8_t *   p_encoded_data)
{
    uint8_t len = sizeof(uint16_t);

    p_date_time->year    = uint16_decode(p_encoded_data);
    p_date_time->month   = p_encoded_data[len++];
    p_date_time->day     = p_encoded_data[len++];
    p_date_time->hours   = p_encoded_data[len++];
    p_date_time->minutes = p_encoded_data[len++];
    p_date_time->seconds = p_encoded_data[len++];

    return len;
}

/* Indicates if operation on TWI has ended. */
static volatile bool m_xfer_done = false;

/* TWI instance. */
static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID);

/* Buffer for samples dates read from RTC. */
static uint8_t m_sample;
static uint8_t sec;
static uint8_t min;
static uint8_t hour;
static uint8_t day;
static uint8_t month;
static uint16_t year;

/**
 * @brief Function for setting time on DS1302 RTC.
 */
/*
void DS1302_set_time(void)
{
    ret_code_t err_code;
    static ble_date_time_t ts = { 2020, 12, 20, 11, 50, 0 };

    // Writing current time to REGs 

    uint8_t secs = (ts.seconds % 10) + ((ts.seconds / 10) << 4);
    err_code = nrf_drv_twi_tx(&m_twi, REG_SECONDS, secs, sizeof(secs), false);
    APP_ERROR_CHECK(err_code);
    while (m_xfer_done == false);

    uint8_t mins = (ts.minutes % 10) + ((ts.minutes / 10) << 4);
    err_code = nrf_drv_twi_tx(&m_twi, REG_MINUTES, mins, sizeof(mins), false);
    APP_ERROR_CHECK(err_code);
    while (m_xfer_done == false);

    uint8_t hours = (ts.hours % 10) + ((ts.hours / 10) << 4);
    err_code = nrf_drv_twi_tx(&m_twi, REG_HOURS, hours, sizeof(hours), false);
    APP_ERROR_CHECK(err_code);
    while (m_xfer_done == false);

    uint8_t dates = (ts.day % 10) + ((ts.day / 10) << 4);
    err_code = nrf_drv_twi_tx(&m_twi, REG_DATES, dates, sizeof(dates), false);
    APP_ERROR_CHECK(err_code);
    while (m_xfer_done == false);

    uint8_t months = ((ts.month + 1) % 10) + (((ts.month + 1) / 10) << 4);
    err_code = nrf_drv_twi_tx(&m_twi, REG_SECONDS, months, sizeof(months), false);
    APP_ERROR_CHECK(err_code);
    while (m_xfer_done == false);

    uint8_t years = ((ts.year - 100) % 10) + (((ts.year - 100) / 10) << 4);
    err_code = nrf_drv_twi_tx(&m_twi, REG_SECONDS, years, sizeof(years), false);
    APP_ERROR_CHECK(err_code);
    while (m_xfer_done == false);
}*/

/*
 * @param data Buffer to copy new data from
 * @return Status of operation (true = success)
 */
bool i2cdev_writeBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t* data) {
	
	/*
	if (NRF_SUCCESS!=nrf_drv_twi_tx(&m_twi,devAddr,&regAddr,1,true))
		return false;
	return NRF_SUCCESS==nrf_drv_twi_tx(&m_twi,devAddr,data,length,false);
	*/
	
	//TODO: This could be improved...
	uint8_t buffer[32];
	buffer[0] = regAddr;
	uint8_t i = 1;
	while(i < (length + 1))
		buffer[i++] = *data++;
	
	return NRF_SUCCESS==nrf_drv_twi_tx(&m_twi,devAddr,buffer,length+1,false);
}

static void get_time() 
{
    uint8_t byte = 0x01;
    ret_code_t err_code;
    m_xfer_done = false;

   // nrf_delay_ms(500);

  //  err_code = nrf_drv_twi_tx(&m_twi, CLOCK_BURST, byte, 1, false);
  //  APP_ERROR_CHECK(err_code);

    NRF_LOG_INFO("burst");

    err_code = nrf_drv_twi_rx(&m_twi, REG_SECONDS, &sec, sizeof(sec));
    APP_ERROR_CHECK(err_code);
    NRF_LOG_INFO("sec. %d", sec);

    //m_xfer_done = false;

    err_code = nrf_drv_twi_rx(&m_twi, REG_MINUTES, &min, sizeof(min));
    APP_ERROR_CHECK(err_code);
    NRF_LOG_INFO("min. %d", min);
/*
    err_code = nrf_drv_twi_rx(&m_twi, REG_HOURS, &hour, sizeof(hour));
    APP_ERROR_CHECK(err_code);
    NRF_LOG_INFO("hour. %d", hour);

    err_code = nrf_drv_twi_rx(&m_twi, REG_DATES, &day, sizeof(day));
    APP_ERROR_CHECK(err_code);
    NRF_LOG_INFO("day. %d", day);

    err_code = nrf_drv_twi_rx(&m_twi, REG_MONTHS, &month, sizeof(month));
    APP_ERROR_CHECK(err_code);
    NRF_LOG_INFO("month. %d", month);

    err_code = nrf_drv_twi_rx(&m_twi, REG_YEARS, &year, sizeof(year));
    APP_ERROR_CHECK(err_code);
    NRF_LOG_INFO("year. %d", year);
*/
}

__STATIC_INLINE void data_handler(uint8_t date)
{
    NRF_LOG_INFO("Date read: %d", date);
}

/**
 * @brief TWI events handler.
 */
void twi_handler(nrf_drv_twi_evt_t const * p_event, void * p_context)
{
    switch (p_event->type)
    {
        case NRF_DRV_TWI_EVT_DONE:
            if (p_event->xfer_desc.type == NRF_DRV_TWI_XFER_RX)
            {
                data_handler(m_sample);
            }
            m_xfer_done = true;
            break;
        default:
            break;
    }
}

/**
 * @brief UART initialization.
 */
void twi_init (void)
{
    ret_code_t err_code;

    const nrf_drv_twi_config_t ds1302_config = {
       .scl                = ARDUINO_SCL_PIN,   // A5 scl -> clk
       .sda                = ARDUINO_SDA_PIN,   // A4 sda -> dat/io  
       .frequency          = NRF_DRV_TWI_FREQ_100K,
       .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
       .clear_bus_init     = false
    };

    err_code = nrf_drv_twi_init(&m_twi, &ds1302_config, twi_handler, NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_INFO("init.");

    nrf_drv_twi_enable(&m_twi);
    NRF_LOG_INFO("enable.");
}


/**
 * @brief Function for main application entry.
 */
int main(void)
{
    APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    NRF_LOG_INFO("\r\nTWI rtc example started.");
    twi_init();
    NRF_LOG_FLUSH();

    while (true)
    {
        nrf_delay_ms(500);
        /*
        do
        {
            __WFE();
        }while (m_xfer_done == false);*/

        NRF_LOG_INFO("Getting time.");
        get_time();   // time is already set just read
        NRF_LOG_FLUSH();
    }
}

/** @} */
