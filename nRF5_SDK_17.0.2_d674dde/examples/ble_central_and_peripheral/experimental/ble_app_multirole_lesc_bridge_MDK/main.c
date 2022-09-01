#include <stdint.h>
#include "app_error.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_crypto.h"
#include "sdk_config.h"

#include "nrf.h"
#include "nrf_drv_saadc.h"
#include "nrf_drv_ppi.h"
#include "nrf_drv_timer.h"

#include "boards.h"
#include "nrf_delay.h"
#include "app_util_platform.h"

#include <stdbool.h>
#include "nordic_common.h"
#include "bsp.h"
#include "nrf_soc.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "ble_advdata.h"

#include "app_timer.h"
#include "nrf_pwr_mgmt.h"

#include "time.h"
#include "nrf_ble_scan.h"
#include "app_uart.h"

#if defined (UART_PRESENT)
#include "nrf_uart.h"
#endif
#if defined (UARTE_PRESENT)
#include "nrf_uarte.h"
#endif
#include "app_fifo.h"
#include "nrf_drv_uart.h"

//#define APP_ADV_DURATION                18000

#define APP_BLE_CONN_CFG_TAG            1                                  /**< A tag identifying the SoftDevice BLE configuration. */


#define DEAD_BEEF                       0xDEADBEEF                         /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

//BLE_ADVERTISING_DEF(m_advertising);                                             /**< Advertising module instance. */

#define SAMPLES_IN_BUFFER 5
volatile uint8_t state = 1;

static const nrf_drv_timer_t m_timer = NRF_DRV_TIMER_INSTANCE(1); // Use TIME1
static nrf_saadc_value_t     m_buffer_pool[2][SAMPLES_IN_BUFFER];
static nrf_ppi_channel_t     m_ppi_channel;
static uint32_t              m_adc_evt_counter;

#define AREA      0xF0
#define MAX_LORA  43 //50    // use airtime calculator to determine the maximum payload
#define base_ts   1605009600

uint8_t payload[MAX_LORA];
uint32_t ini_ts;
int last = 0;
bool send_pkt = false;


static uint8_t m_key[] = {0x6b, 0x65, 0x79};
static uint8_t m_digest[NRF_CRYPTO_HASH_SIZE_SHA256] = {0};
static nrf_crypto_hmac_context_t m_context;

static ble_gap_adv_params_t m_adv_params;                                  /**< Parameters to be passed to the stack when starting advertising. */
static uint8_t              m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET; /**< Advertising handle used to identify an advertising set. */
static uint8_t              m_enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];  /**< Buffer for storing an encoded advertising set. */
#define APP_BEACON_INFO_LENGTH          0x17
uint8_t m_adv_info[APP_BEACON_INFO_LENGTH];
#define NON_CONNECTABLE_ADV_INTERVAL    MSEC_TO_UNITS(250, UNIT_0_625_MS)  /**< The advertising interval for non-connectable advertisement (100 ms). This value can vary between 100ms to 10.24s). */


void timer_handler(nrf_timer_event_t event_type, void * p_context)
{

}


/**@brief Struct that contains pointers to the encoded advertising data. */
static ble_gap_adv_data_t m_adv_data =
{
    .adv_data =
    {
        .p_data = m_enc_advdata,
        .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX
    },
    .scan_rsp_data =
    {
        .p_data = NULL,
        .len    = 0

    }
};

void advertising_init()
{
    uint32_t      err_code;
    ble_advdata_t advdata;
    uint8_t       flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    ble_advdata_manuf_data_t manuf_specific_data;

    sd_ble_gap_adv_stop(m_adv_handle);

    manuf_specific_data.company_identifier = 0x0059; //Nordic


    manuf_specific_data.data.p_data = (uint8_t *) m_adv_info;
    manuf_specific_data.data.size   = APP_BEACON_INFO_LENGTH;

    // Build and set advertising data.
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type             = BLE_ADVDATA_NO_NAME;

    advdata.p_manuf_specific_data = &manuf_specific_data;
    advdata.include_appearance    = false;

    // Initialize advertising parameters (used when starting advertising).
    memset(&m_adv_params, 0, sizeof(m_adv_params));

    m_adv_params.properties.type = BLE_GAP_ADV_TYPE_EXTENDED_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED;
    m_adv_params.p_peer_addr     = NULL;    // Undirected advertisement.
    m_adv_params.filter_policy   = BLE_GAP_ADV_FP_ANY;
    m_adv_params.interval        = NON_CONNECTABLE_ADV_INTERVAL;
    m_adv_params.duration        = 0;       // Never time out.
    m_adv_params.primary_phy     = BLE_GAP_PHY_CODED; // for coded advertisement
    m_adv_params.secondary_phy   = BLE_GAP_PHY_CODED; // for coded advertisement

    err_code = ble_advdata_encode(&advdata, m_adv_data.adv_data.p_data, &m_adv_data.adv_data.len);
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data, &m_adv_params);
    APP_ERROR_CHECK(err_code);

    // set 8 dbm of tx_power (max)
    err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, m_adv_handle, 8);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for starting advertising.
 */
void advertising_start(void)
{
    ret_code_t err_code;

    err_code = sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
    APP_ERROR_CHECK(err_code);
}

void float2hex (float input, uint8_t data_enc[2])
{
    float tmp_frac = input - (int) input;
    float tmp_int = input - tmp_frac;
    data_enc[0] = (int)tmp_int;
    data_enc[1] = (int)(tmp_frac*10);
    NRF_LOG_INFO("input " NRF_LOG_FLOAT_MARKER "\r", NRF_LOG_FLOAT(input));
    NRF_LOG_INFO("int %x", data_enc[0]);
    NRF_LOG_INFO("dec %x", data_enc[1]);
}

void hmac_calc(uint8_t m_data[5])
{
    ret_code_t err_code;
    size_t digest_len = sizeof(m_digest);

    NRF_LOG_INFO("HMAC example started.");

    // Initialize frontend (which also initializes backend).
    err_code = nrf_crypto_hmac_init(&m_context,
                                    &g_nrf_crypto_hmac_sha256_info,
                                    m_key,
                                    sizeof(m_key));
    APP_ERROR_CHECK(err_code);
    // Push all data in one go (could be done repeatedly)
    err_code = nrf_crypto_hmac_update(&m_context, m_data, sizeof(m_data));
    APP_ERROR_CHECK(err_code);
    // Finish calculation
    err_code = nrf_crypto_hmac_finalize(&m_context, m_digest, &digest_len);
    APP_ERROR_CHECK(err_code);

    // Print digest (result).
    NRF_LOG_INFO("Calculated HMAC (length %u:)", digest_len);
    NRF_LOG_HEXDUMP_INFO(m_digest, digest_len);
}

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

/**@brief Function for initializing LEDs. */
static void leds_init(void)
{
    ret_code_t err_code = bsp_init(BSP_INIT_LEDS, NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing timers. */
static void timers_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{
    if (NRF_LOG_PROCESS() == false)
    {
        nrf_pwr_mgmt_run();
    }
}

NRF_BLE_SCAN_DEF(m_scan);
static ble_gap_scan_params_t m_scan_param =               /**< Scan parameters requested for scanning and connection. */
{
    .active        = 0x01,
    .interval      = NRF_BLE_SCAN_SCAN_INTERVAL,
    .window        = NRF_BLE_SCAN_SCAN_WINDOW,
    .timeout       = 0,
    .scan_phys     = BLE_GAP_PHY_CODED,
    .extended      = true,
};

uint8_t burstArray[8];

///////////////

//#define LED_PWR   NRF_GPIO_PIN_MAP(1,9)

#define CE    NRF_GPIO_PIN_MAP(1,11)
#define DAT   NRF_GPIO_PIN_MAP(0,31)
#define SCLK  NRF_GPIO_PIN_MAP(0,2)

#define DS1302_CLOCK_BURST_READ  0xBF



// --------------------------------------------------------
// DS1302_toggleread
//
// A helper function for reading a byte with bit toggle
//
// This function assumes that the SCLK is still high.
//
uint8_t DS1302_toggleread()
{
  uint8_t i, data;
  uint8_t currentBit;

  data = 0;
  for( i = 0; i <= 7; i++)
  {
    // Issue a clock pulse for the next databit.
    // If the 'togglewrite' function was used before
    // this function, the SCLK is already high.
    nrf_gpio_pin_write(SCLK, 1);
    nrf_delay_us(1);

    // Clock down, data is ready after some time.
    nrf_gpio_pin_write(SCLK, 0);
    nrf_delay_us(1);        // tCL=1000ns, tCDD=800ns

    // read bit, and set it in place in 'data' variable
    //bitWrite(data, i, digitalRead(DS1302_IO_PIN));
    currentBit = nrf_gpio_pin_read(DAT);
    data |= (currentBit << i);
  }
  return data;
}

// --------------------------------------------------------
// DS1302_togglewrite
//
// A helper function for writing a byte with bit toggle
//
// The 'release' parameter is for a read after this write.
// It will release the I/O-line and will keep the SCLK high.
//
void DS1302_togglewrite(uint8_t data, uint8_t release)
{
  int i;

  for( i = 0; i <= 7; i++)
  {
    // set a bit of the data on the I/O-line
    // digitalWrite( DS1302_IO_PIN, bitRead(data, i));
    nrf_gpio_pin_write(DAT, (data >> i) & 1);
    nrf_delay_us(1);   // tDC = 200ns

    // clock up, data is read by DS1302
    nrf_gpio_pin_write(SCLK, 1);
    nrf_delay_us(1);   // tCH = 1000ns, tCDH = 800ns

    if( release && i == 7)
    {
      // If this write is followed by a read,
      // the I/O-line should be released after
      // the last bit, before the clock line is made low.
      // This is according the datasheet.
      // I have seen other programs that don't release
      // the I/O-line at this moment,
      // and that could cause a shortcut spike
      // on the I/O-line.
      nrf_gpio_cfg_input(DAT, NRF_GPIO_PIN_PULLUP);

      // For Arduino 1.0.3, removing the pull-up is no longer needed.
      // Setting the pin as 'INPUT' will already remove the pull-up.
      // digitalWrite (DS1302_IO, LOW); // remove any pull-up
    }
    else
    {
      nrf_gpio_pin_write(SCLK, 0);
      nrf_delay_us(1);  // tCL=1000ns, tCDD=800ns
    }
  }
}


// --------------------------------------------------------
// DS1302_start
//
// A helper function to setup the start condition.
//
// An 'init' function is not used.
// But now the pinMode is set every time.
// That's not a big deal, and it's valid.
// At startup, the pins of the Arduino are high impedance.
// Since the DS1302 has pull-down resistors,
// the signals are low (inactive) until the DS1302 is used.
//
void DS1302_start()
{
  nrf_gpio_pin_write(CE, 0);
  nrf_gpio_cfg_output(CE);

  nrf_gpio_pin_write(SCLK, 0);
  nrf_gpio_cfg_output(SCLK);

  nrf_gpio_cfg_output(DAT);
  nrf_gpio_pin_write(CE, 1);  // start the session

  nrf_delay_us(4);  // tCC = 4us
}


// --------------------------------------------------------
// _DS1302_stop
//
// A helper function to finish the communication.
//
void DS1302_stop()
{
  // Set CE low
  nrf_gpio_pin_write(CE, 0);
  nrf_delay_us(4);           // tCWH = 4us
}


////////////////////

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

///////////////

// --------------------------------------------------------
// DS1302_clock_burst_read
//
// This function reads 8 bytes clock data in burst mode
// from the DS1302.
//
// This function may be called as the first function,
// also the pinMode is set.
//
void DS1302_clock_burst_read(uint8_t p[])
{
  int i;

  DS1302_start();

  // Instead of the address,
  // the CLOCK_BURST_READ command is issued
  // the I/O-line is released for the data
  DS1302_togglewrite(DS1302_CLOCK_BURST_READ, true);

  for( i=0; i<8; i++)
  {
    p[i] = DS1302_toggleread();
  }
  DS1302_stop();
}

time_t getEpoch()
{
    struct tm tm;

    DS1302_clock_burst_read(burstArray);

    tm.tm_sec = (burstArray[0] & 0xF) + (burstArray[0] >> 4) * 10;
    tm.tm_min = (burstArray[1] & 0xF) + (burstArray[1] >> 4) * 10;
    tm.tm_hour = (burstArray[2] & 0xF) + (burstArray[2] >> 4) * 10;
    tm.tm_mday = (burstArray[3] & 0xF) + (burstArray[3] >> 4) * 10;
    tm.tm_mon = (burstArray[4] & 0xF) + (burstArray[4] >> 4) * 10 - 1;
    tm.tm_year = (burstArray[6] & 0xF) + (burstArray[6] >> 4) * 10 + 100;

    // convert to timestamp and display (1256729737)
    return mktime(&tm);
}

void convertIntegerToChar(int N, char hex[])
{/*
    // Count digits in number N
    int m = N;
    int digit = 0;

    if (N == 0)
    {
      arr[0]='0';
      arr[1]='\0';
      return;
    }
    while (m) {
        // Increment number of digits
        digit++;
        // Truncate the last
        // digit from the number
        m /= 10;
    }
    // Declare duplicate char array
    char arr1[digit];
    // Separating integer into digits and
    // accomodate it to character array
    int index = 0;
    while (N) {
        // Separate last digit from
        // the number and add ASCII
        // value of character '0' is 48
        arr1[++index] = N % 10 + '0';
        // Truncate the last
        // digit from the number
        N /= 10;
    }
    // Reverse the array for result
    int i;
    for (i = 0; i < index; i++) {
        arr[i] = arr1[index - i];
    }
    // Char array truncate by null
    arr[i] = '\0';
*/
    uint8_t byte;

    byte = (N & 0x00ff);
  //  NRF_LOG_INFO("arr. %x", byte);
    sprintf(hex, "%x", byte);
  //  NRF_LOG_INFO("hex as ascii. %s", hex);
}

void get_data ()
{
  uint8_t cr;
  while (app_uart_get(&cr) != NRF_SUCCESS);
  NRF_LOG_INFO(">%x", cr);
}

void send_command (uint8_t payload[])
{
    uint8_t i;
    const char* base = "at+send=0,2,";
    const char* eoc = "\r\n";

    uint8_t command[3*MAX_LORA + 14];
    char tmp[3*MAX_LORA + 1];
    memset(tmp, 0, (3*MAX_LORA + 1));
    char aa[3];
    int pos = 0;
    for (int i=0;i<MAX_LORA;i++) {
      convertIntegerToChar(payload[i], aa);
    // NRF_LOG_INFO("raw. %s hex %x", aa, (int)aa);
    // sprintf(hex, "%x", (int)aa);
    // NRF_LOG_INFO("hex. %x", hex);
      strcat(tmp, aa);
    }
    NRF_LOG_INFO("payload. %s", tmp);
    strcpy(command, base);
    strcat(command, tmp);
    strcat(command, eoc);
    NRF_LOG_INFO("Sending at command. %s", command);

    for(i = 0; i < strlen(command); i++)
    {
        while (app_uart_put(command[i]) != NRF_SUCCESS);
    }
}

bool send = false;

void generate_payload(uint8_t raw_data[], uint32_t now) //, DateTime now)
    {
       uint8_t num[3];
       uint32_t del;  // in this case only 1 byte max 4 minutes
       uint8_t lowNibble;

       if ((payload[0] == 0) && (payload[1] == 0) && (payload[2] == 0)) {
          NRF_LOG_INFO("No data stored initialize");
          NRF_LOG_INFO("Now: %d", now);
          NRF_LOG_INFO("Base: %d", base_ts);

          ini_ts = now - base_ts;
        //  aux = (uint8_t *)&ini_ts;
        //  memcpy(num, (uint8_t*)&ini_ts, sizeof(ini_ts));
        //  sprintf(num,"%0x",ini_ts);
          NRF_LOG_INFO("ini_ts: %d", ini_ts);
          payload[0] = (ini_ts & 0x000000ff);//num[2];  // timestamp 1
          payload[1] = (ini_ts & 0x0000ff00) >> 8;//num[1];  // timestamp 2
          payload[2] = (ini_ts & 0x00ff0000) >> 16;//num[0];  // timestamp 3
          NRF_LOG_INFO("ini_ts_hex: %d-%d-%d", payload[0], payload[1], payload[2]);
          NRF_LOG_INFO("ini_ts num: %d", payload[0] | (payload[1] << 8) | (payload[2] << 16));
          payload[3] = 0xF0; //raw_data[0]; // area

          lowNibble = (raw_data[1] >> 4)  & 0x0f;

          payload[4] = lowNibble | ((raw_data[0] & 0x0f) << 4);//raw_data[1]; // O2
          payload[5] = raw_data[1] & 0x0f;//raw_data[2]; // O2

          last = 5;
          NRF_LOG_INFO("payload first block");
          for (int i=0; i<MAX_LORA; i++) {
            NRF_LOG_INFO("%d", payload[i]);
          }
       }
       else
       {
        //  payload[0] = 0;  payload[1] = 0;  payload[2] = 0;
          del = now - base_ts - ini_ts; // store the delay (less bytes than store the relative ts)
          NRF_LOG_INFO("init_ori: %d", ini_ts);
          NRF_LOG_INFO("now: %d", now - base_ts);
          NRF_LOG_INFO("delay: %d", del);

          NRF_LOG_INFO("last %d", last);
          lowNibble = (raw_data[1] >> 4)  & 0x0f;
          payload[last+1] = del;

          payload[last+2] = 0xf0;//raw_data[0];
          payload[last+3] = lowNibble | ((raw_data[0] & 0x0f) << 4);//raw_data[1];
          payload[last+4] = raw_data[1] & 0x0f;//raw_data[2];

          last = last + 4;

          NRF_LOG_INFO("tot payload new block ");
          for (int i=0; i<MAX_LORA; i++) {
            NRF_LOG_INFO("%x", payload[i]);
          }
       }
       if (last >= MAX_LORA-1) {
          send_pkt = true;
          NRF_LOG_INFO("Buffer filled");
          //send_command(payload);
          //send_command2();
          /*
          nrf_delay_ms(500);
          app_uart_flush();
          for (int i = 0; i < MAX_LORA; i++) // memset
              payload[i]=0;*/
          last = 0;
       }
      //payload = clear
    }

APP_TIMER_DEF(m_repeated_timer_id);     /** Handler for repeated timer */

APP_TIMER_DEF(m_repeated_timer_id2);     /** Handler for repeated timer */
uint32_t counter = 0;

//APP_TIMER_DEF(m_repeated_timer_adv);    /** Handler for repeated timer */

/**@brief Timeout handler for the repeated timer.
 */
static void repeated_timer_handler(void * p_context)
{
    send = true;
}

void modify_adv(ble_gap_evt_adv_report_t *p_adv_report)
{
  NRF_LOG_INFO("modify legacy adv");
  for (int i = 0; i<5; i++) {
    m_adv_info[i] = p_adv_report->data.p_data[i+4];
    NRF_LOG_INFO("adv %x", m_adv_info[i]);
  }
}

static void repeated_timer_check_last_handler(void * p_context)
{
  NRF_LOG_INFO("checking last adv");
  NRF_LOG_INFO("current %d - last counter %d", app_timer_cnt_get(), counter);
  if ((app_timer_cnt_get() - counter) > APP_TIMER_TICKS(5000)) {
    for (int i = 0; i<6; i++) {
      m_adv_info[i] = 0;
      NRF_LOG_INFO("adv empty %x", m_adv_info[i]);
    }
    //advertising_init();
    //advertising_start();
  }
}


/**@brief Create timers.
 */
static void create_timers()
{
    ret_code_t err_code;

    // Create timers
    err_code = app_timer_create(&m_repeated_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                repeated_timer_handler);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_repeated_timer_id2,
                                APP_TIMER_MODE_REPEATED,
                                repeated_timer_check_last_handler);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t const * p_ble_evt)
{
    uint32_t              err_code;
    const ble_gap_evt_t * p_gap_evt = &p_ble_evt->evt.gap_evt;
    uint8_t i,j = 0;
    uint8_t raw_data[35];
    bool is_valid = false;

    NRF_LOG_INFO("ble evt");

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_ADV_REPORT:
        {
            const ble_gap_evt_adv_report_t *p_adv_report = &p_gap_evt->params.adv_report;
            while (i < p_adv_report->data.len) {
              if (p_adv_report->data.p_data[2] == 89 && p_adv_report->data.p_data[3] == 0 && i > 3)
              { // filter by company identifier 00 59 aka Nordic ASA
                //NRF_LOG_INFO("Raw Data Adv: %x", p_adv_report->data.p_data[i]);
                raw_data[j] = p_adv_report->data.p_data[i];
                j++;
                is_valid = true;                 // test hmac if not discard
              }
              i++;
            }
            i=0; j=0;
            while (i<BLE_GAP_ADDR_LEN) {
              NRF_LOG_INFO("MAC: %x", p_adv_report->peer_addr.addr[i]);
              i++;
            } i = 0;
            NRF_LOG_INFO("RSSI: %d", p_adv_report->rssi);
            if (is_valid) {

                counter = app_timer_cnt_get();
                NRF_LOG_INFO("last adv counter %d", counter);
                modify_adv(p_adv_report);   //generate legacy for smartphone
                advertising_init();
                advertising_start();
            }
            if (is_valid && send) {
              NRF_LOG_INFO("Parse Adv data to generate LoRaWAN payload");
              NRF_LOG_INFO("ts: %d", (uint32_t)getEpoch());
              NRF_LOG_INFO("raw data: %x-%x-%x", raw_data[0], raw_data[1], raw_data[2]);
              generate_payload(raw_data, (uint32_t)getEpoch());
              send = false;
            }
            break;
        }
        default:
            break;
    }
}

/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the BLE Stack event interrupt handler after a BLE stack
 *          event has been received.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t const * p_ble_evt, void * p_context)
{
    UNUSED_PARAMETER(p_context);
    on_ble_evt(p_ble_evt);
}

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    NRF_SDH_BLE_OBSERVER(m_ble_evt_observer, 1, ble_evt_dispatch, NULL);
}

/**@brief Function for handling events from the Scanning Module.
 *
 * @param[in]   event   Event generated.
 */
static void scan_evt_handler(scan_evt_t const * p_scan_evt)
{
    ret_code_t err_code;
    NRF_LOG_INFO("Scan evt.");

    switch(p_scan_evt->scan_evt_id)
    {
        case NRF_BLE_SCAN_EVT_SCAN_TIMEOUT:
        {
            NRF_LOG_INFO("Scan timed out.");
         //   scan_start();
        } break;
        default:
          break;
    }
}

/**@brief Function for initializing scanning.
 */
static void scan_init(void)
{
    ret_code_t          err_code;
    nrf_ble_scan_init_t init_scan;
    /*
    ble_uuid_t uuid =
    {
        .uuid = TARGET_UUID,
        .type = BLE_UUID_TYPE_BLE,
    };*/

    memset(&init_scan, 0, sizeof(init_scan));

    init_scan.connect_if_match = true;
    init_scan.conn_cfg_tag     = APP_BLE_CONN_CFG_TAG;
    init_scan.p_scan_param     = &m_scan_param;

    err_code = nrf_ble_scan_init(&m_scan, &init_scan, scan_evt_handler);
    APP_ERROR_CHECK(err_code);
/*
    err_code = nrf_ble_scan_filter_set(&m_scan, SCAN_UUID_FILTER, &uuid);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_scan_filters_enable(&m_scan,
                                           NRF_BLE_SCAN_UUID_FILTER,
                                           false);
    APP_ERROR_CHECK(err_code);*/
}

/**@brief Function for starting the scanning.
 */
static void scan_start(void)
{
    ret_code_t err_code;

    err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_SCAN_INIT, BLE_CONN_HANDLE_INVALID, 8);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_scan_params_set(&m_scan, &m_scan_param);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_scan_start(&m_scan);
    APP_ERROR_CHECK(err_code);
}

//static uint8_t m_data[] = {0x35, 0x2E, 0x02, 0x15, 0x01};


/*
static uint8_t m_expected_digest[NRF_CRYPTO_HASH_SIZE_SHA256] =
{
    0x6e, 0x9e, 0xf2, 0x9b, 0x75, 0xff, 0xfc, 0x5b, 0x7a, 0xba, 0xe5, 0x27, 0xd5, 0x8f, 0xda, 0xdb,
    0x2f, 0xe4, 0x2e, 0x72, 0x19, 0x01, 0x19, 0x76, 0x91, 0x73, 0x43, 0x06, 0x5f, 0x58, 0xed, 0x4a,

};*/

static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

/**@brief Function for handling Scaning events.
 *
 * @param[in]   p_scan_evt   Scanning event.
 */
 /*
static void scan_evt_handler(scan_evt_t const * p_scan_evt)
{
    ret_code_t err_code;

    NRF_LOG_INFO("RSSI: %d", p_scan_evt->p_scan_params);  //.params.filter_match);//->p_adv_report.rssi);

    switch(p_scan_evt->scan_evt_id)
    {
        case NRF_BLE_SCAN_EVT_CONNECTING_ERROR:
            err_code = p_scan_evt->params.connecting_err.err_code;
            APP_ERROR_CHECK(err_code);
            break;
        default:
          break;
    }
}*/



//#define INPUT_LED              NRF_GPIO_PIN_MAP(1,9)

#define MAX_TEST_DATA_BYTES     (15U)                /**< max number of test bytes to be used for tx and rx. */
#define UART_TX_BUF_SIZE 256                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 256                         /**< UART RX buffer size. */

/* When UART is used for communication with the host do not use flow control.*/
#define UART_HWFC APP_UART_FLOW_CONTROL_DISABLED



void uart_handle(app_uart_evt_t * p_event)
{
    uint8_t cr;

    if (p_event->evt_type == APP_UART_DATA_READY)
    {
     // app_uart_get(&cr);
     // NRF_LOG_INFO("data");// %c", cr);
     // app_uart_get(&cr);
      //app_uart_get(&cr);
     // if (cr == 0x4F) {// Means O
     // while (app_uart_get(&cr) != NRF_SUCCESS) ;
     // NRF_LOG_INFO("Response %c", cr);

    }
    else if (p_event->evt_type == APP_UART_COMMUNICATION_ERROR)
    {
        NRF_LOG_INFO("comm err");
        APP_ERROR_HANDLER(p_event->data.error_communication);
    }
    else if (p_event->evt_type == APP_UART_FIFO_ERROR)
    {
        NRF_LOG_INFO("uart err");
        APP_ERROR_HANDLER(p_event->data.error_code);
    }
}

void init_uart()
{
    uint32_t err_code;

    const app_uart_comm_params_t comm_params =
      {
          RX_PIN_NUMBER,
          TX_PIN_NUMBER,
          RTS_PIN_NUMBER,
          CTS_PIN_NUMBER,
          APP_UART_FLOW_CONTROL_DISABLED,
          false,
#if defined (UART_PRESENT)
          NRF_UART_BAUDRATE_115200
#else
          NRF_UARTE_BAUDRATE_115200
#endif
      };

    APP_UART_FIFO_INIT(&comm_params,
                         UART_RX_BUF_SIZE,
                         UART_TX_BUF_SIZE,
                         uart_handle,
                         APP_IRQ_PRIORITY_LOWEST,
                         err_code);

    APP_ERROR_CHECK(err_code);
    NRF_LOG_INFO("Debug logging for UART over RTT started.");

     // printf("\r\nUART example started.\r\n");
}

//uint8_t * send_cmd = (uint8_t *) ("at+version\r\n");



//////////////
//////////////

int main(void)
{
    uint32_t err_code = NRF_SUCCESS;
  //  time_t epoch;

    init_uart();
    log_init();

/*
    NRF_LOG_INFO("DS1302 example started.");
    struct tm tm;

    while (1) {
      DS1302_clock_burst_read(burstArray);
      NRF_LOG_INFO("sec %d", decode(burstArray[0]));
      NRF_LOG_INFO("min %d", decode(burstArray[1]));
      NRF_LOG_INFO("hr %d", decodeH(burstArray[2]));
      NRF_LOG_INFO("date %d", decode(burstArray[3]));
      NRF_LOG_INFO("mon %d", decode(burstArray[4]));
      NRF_LOG_INFO("year %d", decodeY(burstArray[6])+2000);

      tm.tm_sec = (burstArray[0] & 0xF) + (burstArray[0] >> 4) * 10;
      tm.tm_min = (burstArray[1] & 0xF) + (burstArray[1] >> 4) * 10;
      tm.tm_hour = (burstArray[2] & 0xF) + (burstArray[2] >> 4) * 10;
      tm.tm_mday = (burstArray[3] & 0xF) + (burstArray[3] >> 4) * 10;
      tm.tm_mon = (burstArray[4] & 0xF) + (burstArray[4] >> 4) * 10 - 1;
      tm.tm_year = (burstArray[6] & 0xF) + (burstArray[6] >> 4) * 10 + 100;

    // convert to timestamp and display (1256729737)
      epoch = mktime(&tm);
      NRF_LOG_INFO("epoch %d", epoch);

      nrf_delay_ms(1000);
      NRF_LOG_FLUSH();
    }*/

    timers_init();
    leds_init();
    power_management_init();
    ble_stack_init();

    ////////

//    nrf_delay_ms(23000); // let the wistrio start
/*
    while (1)
    {
      send_command2();
      nrf_delay_ms(3000);
      app_uart_flush();
    }*/

    ////////
    create_timers();
    err_code = app_timer_start(m_repeated_timer_id, APP_TIMER_TICKS(1000), NULL);
    err_code = app_timer_start(m_repeated_timer_id2, APP_TIMER_TICKS(4000), NULL);

/*
   nrf_gpio_cfg_output(INPUT_LED);
   nrf_gpio_pin_clear(INPUT_LED);
   while(1)
   {
       nrf_gpio_pin_toggle(INPUT_LED);
       nrf_delay_ms(500);
   }
*/
  //  NRF_LOG_INFO("Beacon example started.");

    // do ble stack before crypto
/*    saadc_init();
    saadc_sampling_event_init();
    saadc_sampling_event_enable();*/
 //   NRF_LOG_INFO("SAADC HAL simple example started.");


    NRF_LOG_INFO("Initializing nrf_crypto.");
    err_code = nrf_crypto_init();
    APP_ERROR_CHECK(err_code);
    NRF_LOG_INFO("Initialized nrf_crypto.");
  //  APP_ERROR_CHECK(err_code);

  //  err_code = nrf_crypto_init();
  //  NRF_LOG_INFO("m expected %x", m_expected_digest);
    // Compare calculated digest with the expected digest. not in this node only on reciever
    /*
    if (memcmp(m_digest, m_expected_digest, digest_len) == 0)
    {
        NRF_LOG_INFO("HMAC example executed successfully.");
    }
    else
    {
        NRF_LOG_ERROR("HMAC example failed!!!");
        while(1);
    }*/


   advertising_init();
   advertising_start();

  // scan_init();
  // scan_start();
/*
    for (;;)
    {                                         // let the wistrio start
      if (send_pkt == true)// && app_timer_cnt_get() > APP_TIMER_TICKS(28000))
      {
        send_command(payload);
        send_pkt = false;
        for (int i = 0; i < MAX_LORA; i++) // memset
          payload[i]=0;
      }
      idle_state_handle();
      nrf_pwr_mgmt_run();
      NRF_LOG_FLUSH();
    }*/
}

/**
 *@}
 **/
