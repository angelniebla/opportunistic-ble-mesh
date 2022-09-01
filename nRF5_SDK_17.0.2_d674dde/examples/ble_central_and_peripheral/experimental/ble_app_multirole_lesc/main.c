

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



#define APP_ADV_INTERVAL                80                                      /**< The advertising interval (in units of 0.625 ms. This value corresponds to 50 ms). */

//#define APP_ADV_DURATION                18000

#define APP_BLE_CONN_CFG_TAG            1                                  /**< A tag identifying the SoftDevice BLE configuration. */

#define NON_CONNECTABLE_ADV_INTERVAL    MSEC_TO_UNITS(100, UNIT_0_625_MS)  /**< The advertising interval for non-connectable advertisement (100 ms). This value can vary between 100ms to 10.24s). */

#define APP_BEACON_INFO_LENGTH          0x17                               /**< Total length of information advertised by the Beacon. */
#define APP_ADV_DATA_LENGTH             0x15                               /**< Length of manufacturer specific data in the advertisement. */
#define APP_DEVICE_TYPE                 0x02                               /**< 0x02 refers to Beacon. */
#define APP_MEASURED_RSSI               0xC3                               /**< The Beacon's measured RSSI at 1 meter distance in dBm. */
#define APP_COMPANY_IDENTIFIER          0x0059                             /**< Company identifier for Nordic Semiconductor ASA. as per www.bluetooth.org. */
#define APP_MAJOR_VALUE                 0x01, 0x02                         /**< Major value used to identify Beacons. */
#define APP_MINOR_VALUE                 0x03, 0x04                         /**< Minor value used to identify Beacons. */
#define APP_BEACON_UUID                 0x01, 0x12, 0x23, 0x34, \
                                        0x45, 0x56, 0x67, 0x78, \
                                        0x89, 0x9a, 0xab, 0xbc, \
                                        0xcd, 0xde, 0xef, 0xf0            /**< Proprietary UUID for Beacon. */

#define DEAD_BEEF                       0xDEADBEEF                         /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#if defined(USE_UICR_FOR_MAJ_MIN_VALUES)
#define MAJ_VAL_OFFSET_IN_BEACON_INFO   18                                 /**< Position of the MSB of the Major Value in m_beacon_info array. */
#define UICR_ADDRESS                    0x10001080                         /**< Address of the UICR register used by this example. The major and minor versions to be encoded into the advertising data will be picked up from this location. */
#endif

//BLE_ADVERTISING_DEF(m_advertising);                                             /**< Advertising module instance. */

static ble_gap_adv_params_t m_adv_params;                                  /**< Parameters to be passed to the stack when starting advertising. */
static uint8_t              m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET; /**< Advertising handle used to identify an advertising set. */
static uint8_t              m_enc_advdata[41];//BLE_GAP_ADV_SET_DATA_SIZE_MAX];  /**< Buffer for storing an encoded advertising set. */


#define SAMPLES_IN_BUFFER 8
#define SAADC_SAMPLE_INTERVAL 500

volatile uint8_t state = 1;

static const nrf_drv_timer_t m_timer = NRF_DRV_TIMER_INSTANCE(1); // Use TIME1
static nrf_saadc_value_t     m_buffer_pool[2][SAMPLES_IN_BUFFER];
static nrf_ppi_channel_t     m_ppi_channel;
static uint32_t              m_adc_evt_counter;
//static const nrf_drv_timer_t m_timer2 = NRF_DRV_TIMER_INSTANCE(2); // Use TIME2

#define AREA    0xF0

/**@brief Struct that contains pointers to the encoded advertising data. */
static ble_gap_adv_data_t m_adv_data =
{
    .adv_data =
    {
        .p_data = m_enc_advdata,
        .len    = 41 //BLE_GAP_ADV_SET_DATA_SIZE_MAX
    },
    .scan_rsp_data =
    {
        .p_data = NULL,
        .len    = 0

    }
};

static uint8_t m_key[] = {0x6b, 0x65, 0x79};
static uint8_t m_digest[NRF_CRYPTO_HASH_SIZE_SHA256] = {0};
static nrf_crypto_hmac_context_t m_context;

uint8_t data_and_hash[] =                    /**< Information advertised by the Beacon. */
{
    0xF1, 0x51, 0x1C, 0x15, 0x92, 0x63,   // info, will be dynamical modified from saadc and sensors
    0xFB, 0x4C, 0xE3, 0xF8, 0xA1, 0xF0, 0x36, 0x9E, // sha256 hash (also modified)
    0x51, 0x05, 0xE9, 0xBC, 0xE6, 0x07, 0x2C, 0x60, //
    0x19, 0x88, 0xAA, 0x31, 0x04, 0x23, 0xFE, 0x14, //
    0xCA, 0x0B, 0xEB, 0x5A, 0xE6, 0x3A, 0x05, 0xB2  //
};

////////////

#define DHT         NRF_GPIO_PIN_MAP(1,11) // D2 - P1.11

float humidity = 0;
float temperature = 0;

APP_TIMER_DEF(m_repeated_timer_id);     /** Handler for repeated timer */

uint32_t expectPulse(bool level) {
  uint32_t count = 0;
  while (nrf_gpio_pin_read(DHT) == level) {
    if (count >= 500) {
      return -1; // Exceeded timeout, fail.
    }
    count++;
  }
  return count;
}

int read_dht()
{
  uint8_t data[5];
  bool lastresult;
  uint32_t cycles[80];

  data[0] = data[1] = data[2] = data[3] = data[4] = 0;

  nrf_gpio_cfg_output(DHT);
  nrf_gpio_pin_write(DHT, 0); 	// pull down for 20 ms for a smooth and nice wake up 
  nrf_delay_ms(20);
	 		
  nrf_gpio_pin_write(DHT, 1);   // pull up for 40 us for a gentile asking for data
  nrf_delay_us(40);

  nrf_gpio_cfg_input(DHT, NRF_GPIO_PIN_PULLUP); // change to input mode

 // nrf_delay_us(12);
  // First expect a low signal for ~80 microseconds followed by a high signal
  // for ~80 microseconds again.
    if (expectPulse(0) == -1) {
      //NRF_LOG_INFO("DHT timeout waiting for start signal low pulse.");
      lastresult = false;
    }
    if (expectPulse(1) == -1) {
     // NRF_LOG_INFO("DHT timeout waiting for start signal high pulse.");
      lastresult = false;
    }
  
    // Now read the 40 bits sent by the sensor.  Each bit is sent as a 50
    // microsecond low pulse followed by a variable length high pulse.  If the
    // high pulse is ~28 microseconds then it's a 0 and if it's ~70 microseconds
    // then it's a 1.  We measure the cycle count of the initial 50us low pulse
    // and use that to compare to the cycle count of the high pulse to determine
    // if the bit is a 0 (high state cycle count < low state cycle count), or a
    // 1 (high state cycle count > low state cycle count). Note that for speed
    // all the pulses are read into a array and then examined in a later step.
    for (int i = 0; i < 80; i += 2) {
      cycles[i] = expectPulse(0);
      cycles[i + 1] = expectPulse(1);
    }
  // Timing critical code is now complete.

  // Inspect pulses and determine which ones are 0 (high state cycle count < low
  // state cycle count), or 1 (high state cycle count > low state cycle count).
  for (int i = 0; i < 40; ++i) {
    uint32_t lowCycles = cycles[2 * i];
    //NRF_LOG_INFO("low cycles %x", cycles[2 * i]);
    uint32_t highCycles = cycles[2 * i + 1];
    //NRF_LOG_INFO("high cycles %x", cycles[2 * i + 1]);
    if ((lowCycles == -1) || (highCycles == -1)) {
     // NRF_LOG_INFO("DHT timeout waiting for pulse.");
      lastresult = false;
    }
    data[i / 8] <<= 1;
    // Now compare the low and high cycle times to see if the bit is a 0 or 1.
    if (highCycles > lowCycles) {
      // High cycles are greater than 50us low cycle count, must be a 1.
      data[i / 8] |= 1;
    }
    // Else high cycles are less than (or equal to, a weird case) the 50us low
    // cycle count so this must be a zero.  Nothing needs to be changed in the
    // stored data.
  }

// == verify if checksum is ok ===========================================
// Checksum is the sum of Data 8 bits masked out 0xFF
	
  if (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
  {
  //  NRF_LOG_INFO( "checksum OK\n" );
    
// == get humidity from Data[0] and Data[1] ==========================
    humidity = data[0];
    humidity *= 0x100;					// >> 8
    humidity += data[1];
    humidity /= 10;						// get the decimal

    humidity = humidity - (100 - humidity)*1.1; // not very accurate values due to read cycles problems for now  
                                                // it will be enought at least get more or less well predictions

// == get temp from Data[2] and Data[3]
    temperature = data[2] & 0x7F;	
    temperature *= 0x100;				// >> 8
    temperature += data[3];
    temperature /= 10;

    if( data[2] & 0x80 ) 			// negative temp
      temperature *= -1;

   // NRF_LOG_INFO("Humidity: " NRF_LOG_FLOAT_MARKER "\r", NRF_LOG_FLOAT(humidity));
  //  NRF_LOG_INFO("Temperature: " NRF_LOG_FLOAT_MARKER "\r", NRF_LOG_FLOAT(temperature));
  }
  //else 
  //  NRF_LOG_INFO( "checksum ERROR\n" );
}

// macro array of nibbles (the area and decimal values are only one nibble)

//#define Array(i) (i%2 ? (nibbles[i/2] & 0x0F) : ((nibbles[i/2] & 0xF0) >> 4))
//#define Array(i) (unsigned)((pseudo_array & (0x0f << (uint8_t)(4*i))) >> (uint8_t)(4*i))

/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
void advertising_init()
{
    uint32_t      err_code;
    ble_advdata_t advdata;
    uint8_t       flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    ble_advdata_manuf_data_t manuf_specific_data;
    sd_ble_gap_adv_stop(m_adv_handle);
    manuf_specific_data.company_identifier = APP_COMPANY_IDENTIFIER;
 //   NRF_LOG_INFO("raw %x-%x", data_and_hash[0], data_and_hash[1]);

    manuf_specific_data.data.p_data = (uint8_t *) data_and_hash; //m_beacon_info;
    manuf_specific_data.data.size   = 35;//APP_BEACON_INFO_LENGTH;

    // Build and set advertising data.
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type             = BLE_ADVDATA_NO_NAME;
    // avoid the flags for more manuf data
    advdata.p_manuf_specific_data = &manuf_specific_data;
    advdata.include_appearance    = false;

    // Initialize advertising parameters (used when starting advertising).
    memset(&m_adv_params, 0, sizeof(m_adv_params));

    m_adv_params.p_peer_addr     = NULL;    // Undirected advertisement.
    m_adv_params.filter_policy   = BLE_GAP_ADV_FP_ANY;
    m_adv_params.interval        = APP_ADV_INTERVAL;
    m_adv_params.duration        = 0;       // Never time out.

    m_adv_params.properties.type = BLE_GAP_ADV_TYPE_EXTENDED_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED; // lowest bitrate 125kbps
    m_adv_params.primary_phy     = BLE_GAP_PHY_CODED; // for coded advertisement
    m_adv_params.secondary_phy   = BLE_GAP_PHY_CODED; // for coded advertisement

    m_adv_params.filter_policy   = BLE_GAP_ADV_FP_ANY;

    err_code = ble_advdata_encode(&advdata, m_adv_data.adv_data.p_data, &m_adv_data.adv_data.len);
//    err_code = ble_advdata_encode(&advdata, m_adv_data.adv_data.p_data, &m_adv_data.adv_data.len);
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

void timer_handler(nrf_timer_event_t event_type, void * p_context)
{

}

void saadc_sampling_event_init(void)
{
    ret_code_t err_code;

    err_code = nrf_drv_ppi_init();
    APP_ERROR_CHECK(err_code);

    nrf_drv_timer_config_t timer_cfg = NRF_DRV_TIMER_DEFAULT_CONFIG;
    timer_cfg.bit_width = NRF_TIMER_BIT_WIDTH_32;
    err_code = nrf_drv_timer_init(&m_timer, &timer_cfg, timer_handler);
    APP_ERROR_CHECK(err_code);

    /* sample rate every ms */
    uint32_t ticks = nrf_drv_timer_ms_to_ticks(&m_timer, SAADC_SAMPLE_INTERVAL);
    nrf_drv_timer_extended_compare(&m_timer,
                                   NRF_TIMER_CC_CHANNEL0,
                                   ticks,
                                   NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK,
                                   false);
    nrf_drv_timer_enable(&m_timer);
    uint32_t timer_compare_event_addr = nrf_drv_timer_compare_event_address_get(&m_timer,
                                                                                NRF_TIMER_CC_CHANNEL0);
    uint32_t saadc_sample_task_addr   = nrf_drv_saadc_sample_task_get();

    /* setup ppi channel so that timer compare event is triggering sample task in SAADC */
    err_code = nrf_drv_ppi_channel_alloc(&m_ppi_channel);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_ppi_channel_assign(m_ppi_channel,
                                          timer_compare_event_addr,
                                          saadc_sample_task_addr);
    APP_ERROR_CHECK(err_code);
}


void saadc_sampling_event_enable(void)
{
    ret_code_t err_code = nrf_drv_ppi_channel_enable(m_ppi_channel);

    APP_ERROR_CHECK(err_code);
}

void float2hex (float input, uint8_t data_enc[2])
{
    float tmp_frac = input - (int) input;
    float tmp_int = input - tmp_frac;
    data_enc[0] = (int)tmp_int;
    data_enc[1] = (int)(tmp_frac*10);
  //  NRF_LOG_INFO("input " NRF_LOG_FLOAT_MARKER "\r", NRF_LOG_FLOAT(input));
  //  NRF_LOG_INFO("int %x", data_enc[0]);
  //  NRF_LOG_INFO("dec %x", data_enc[1]);
}

void hmac_calc(uint8_t m_data[5])
{
    ret_code_t err_code;
    size_t digest_len = sizeof(m_digest);

 //   NRF_LOG_INFO("HMAC example started.");

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
  //  NRF_LOG_INFO("Calculated HMAC (length %u:)", digest_len);
//    NRF_LOG_HEXDUMP_INFO(m_digest, digest_len);
}

/* Some example
  f1 51 1c 15 92

  area f
  O2 15 1  = 21.1
  temp 1c 1 = 28.1
  hum 59 2 = 89.2

  f = 15
  f1 = 241
*/
void modify_adv(float dataO2, float tmp, float hum, int batt)
{
  uint8_t aux[2];
  uint8_t values[5];
 // uint8_t nibbles[10];  // 5 bytes data eq 10 nibbles
  uint8_t nibble1;
  uint8_t nibble2;

  float2hex(dataO2, aux);
//  NRF_LOG_INFO("byte value O2 int %x", aux[0]);
//  NRF_LOG_INFO("byte value O2 dec %x", aux[1]);
//  NRF_LOG_INFO("1B nibble1 %x - nibble2 %x", (uint8_t)(aux[0] & 0x0F), (uint8_t)((aux[0] & 0xF0) >> 4));
//  NRF_LOG_INFO("2B nibble1 %x - nibble2 %x", (uint8_t)(aux[1] & 0x0F), (uint8_t)((aux[1] & 0xF0) >> 4));
//  NRF_LOG_INFO("1B 2B nibble1 %x", (uint8_t)((aux[0] & 0x0F) << 4 | (aux[1] & 0x0F)));
  if (aux[0] > 15) { // more than one nibble for code
    data_and_hash[0] = AREA | (uint8_t)((aux[0] & 0xF0) >> 4); // area nibble (xF) plus O2 2 nibble
    data_and_hash[1] = (uint8_t)((aux[0] & 0x0F) << 4 |
                                 (aux[1] & 0x0F)); // O2 1 nibble plus decimal nibble
  }
  else { // pad with 0
    data_and_hash[0] = AREA;
    data_and_hash[1] = (uint8_t)((aux[0] & 0x0F) << 4 |
                       (aux[1] & 0x0F));
  }

  float2hex(tmp, aux);
//  NRF_LOG_INFO("byte value tmp int %x", aux[0]);
//  NRF_LOG_INFO("byte value tmp dec %x", aux[1]);

  data_and_hash[2] = aux[0]; // is the complete byte
  float2hex(tmp, aux);

  uint8_t high = (aux[1] >> 4) & 0x0F; // decimal is only one nibble
  uint8_t lowT = aux[1] & 0x0F;         

 // NRF_LOG_INFO("high %x low %x", high, lowT);

  float2hex(hum, aux);

 // NRF_LOG_INFO("byte value hum int %x", aux[0]);
 // NRF_LOG_INFO("byte value hum dec %x", aux[1]);

  uint8_t highH = (aux[0] >> 4) & 0x0F;
  uint8_t lowX = aux[0] & 0x0F;         // first nibble integer part hum
//  NRF_LOG_INFO("low0 %x high0 %x", lowX, highH);

  high = (aux[1] >> 4) & 0x0F;
  uint8_t lowD = aux[1] & 0x0F;         // first nibble integer part hum
//  NRF_LOG_INFO("low1 %x high1 %x", lowD, high);

  data_and_hash[3] = (lowT << 4) | highH;

  high = (aux[0] >> 4) & 0x0F;        // second nibble integer part hum

  data_and_hash[4] = (lowX << 4) | lowD;

  data_and_hash[5] = batt;
/*
  NRF_LOG_INFO("1 byte %x", data_and_hash[0]);
  NRF_LOG_INFO("2 byte %x", data_and_hash[1]);
  NRF_LOG_INFO("3 byte %x", data_and_hash[2]);
  NRF_LOG_INFO("4 byte %x", data_and_hash[3]);
  NRF_LOG_INFO("5 byte %x", data_and_hash[4]);
  NRF_LOG_INFO("6 byte %x", data_and_hash[5]);
*/
  for (int i=0; i<6; i++)
    values[i] = data_and_hash[i];
  hmac_calc(values);
  for (int i=6; i<38; i++)
    data_and_hash[i] = m_digest[i-5];
}

void saadc_callback(nrf_drv_saadc_evt_t const * p_event)
{

    if (p_event->type == NRF_DRV_SAADC_EVT_DONE)
    {
        float averageAIN1 = 0;
        float averageAIN4 = 0;
        ret_code_t err_code;

        err_code = nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, SAMPLES_IN_BUFFER);
        APP_ERROR_CHECK(err_code);

        int i;
     //   NRF_LOG_INFO("ADC event number: %d", (int)m_adc_evt_counter);

        for (i = 0; i < SAMPLES_IN_BUFFER; i++)
        {
          if ((i % 2) != 0 )
              averageAIN4 = averageAIN4 + p_event->data.done.p_buffer[i];
          else
              averageAIN1 = averageAIN1 + p_event->data.done.p_buffer[i];
        }
     //   NRF_LOG_INFO("sum " NRF_LOG_FLOAT_MARKER "\r", NRF_LOG_FLOAT(averageAIN1));
        averageAIN1 = averageAIN1 / (SAMPLES_IN_BUFFER / 2);
     //   NRF_LOG_INFO("avg " NRF_LOG_FLOAT_MARKER "\r", NRF_LOG_FLOAT(averageAIN1));

        averageAIN4 = averageAIN4 / (SAMPLES_IN_BUFFER / 2);
    //    NRF_LOG_INFO("avg2 " NRF_LOG_FLOAT_MARKER "\r", NRF_LOG_FLOAT(averageAIN4));

        averageAIN1 = (averageAIN1 * 3300.0F ) / (4096.0F);   // voltage value
   //     NRF_LOG_INFO("voltage " NRF_LOG_FLOAT_MARKER "\r", NRF_LOG_FLOAT(averageAIN1));

        averageAIN1 = averageAIN1 / 38.5F;                     // convert to percent

     //   NRF_LOG_INFO("o2 value " NRF_LOG_FLOAT_MARKER "\r", NRF_LOG_FLOAT(averageAIN1));
    //    NRF_LOG_INFO("bat value " NRF_LOG_FLOAT_MARKER "\r", NRF_LOG_FLOAT(averageAIN4));

        // modify advertising data
        modify_adv(averageAIN1, temperature, humidity, averageAIN4 / 19.0F);
        advertising_init();
        advertising_start();
        m_adc_evt_counter++;
    }
}

//////
static void create_timers()
{
    ret_code_t err_code;

    // Create timers
    err_code = app_timer_create(&m_repeated_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                read_dht);
    APP_ERROR_CHECK(err_code);
}

////////////

void saadc_init(void) 
{

    ret_code_t err_code;

    nrf_saadc_channel_config_t channel_config0;
    nrf_saadc_channel_config_t channel_config1;
    nrf_drv_saadc_config_t saadc_config;

    //Configure SAADC
    saadc_config.resolution = NRF_SAADC_RESOLUTION_12BIT;                       //Set SAADC resolution to 12-bit. This will make the SAADC output values from 0 (when input voltage is 0V) to 2^12=2048 (when input voltage is 3.6V for channel gain setting of 1/6).
    saadc_config.oversample = NRF_SAADC_OVERSAMPLE_DISABLED;                                 //Set oversample to 4x. This will make the SAADC output a single averaged value when the SAMPLE task is triggered 4 times.
    saadc_config.interrupt_priority = APP_IRQ_PRIORITY_LOW;                     //Set SAADC interrupt to low priority.
	
/*
#define NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(PIN_P) \
{                                                   \
    .resistor_p = NRF_SAADC_RESISTOR_DISABLED,      \
    .resistor_n = NRF_SAADC_RESISTOR_DISABLED,      \
    .gain       = NRF_SAADC_GAIN1_6,                \
    .reference  = NRF_SAADC_REFERENCE_INTERNAL,     \
    .acq_time   = NRF_SAADC_ACQTIME_10US,           \
    .mode       = NRF_SAADC_MODE_SINGLE_ENDED,      \
    .burst      = NRF_SAADC_BURST_DISABLED,         \
    .pin_p      = (nrf_saadc_input_t)(PIN_P),       \
    .pin_n      = NRF_SAADC_INPUT_DISABLED          \
}
*/

/// values from Arduino Core for AR_INTERNAL referennce
/// https://github.com/arduino/ArduinoCore-nRF528x-mbedos/blob/6216632cc70271619ad43547c804dabb4afa4a00/variants/ARDUINO_NANO33BLE/variant.cpp

    channel_config0.resistor_p = NRF_SAADC_RESISTOR_DISABLED;
    channel_config0.resistor_n = NRF_SAADC_RESISTOR_DISABLED;
    channel_config0.gain       = NRF_SAADC_GAIN1; //1_6
    channel_config0.acq_time   = NRF_SAADC_ACQTIME_10US;
    channel_config0.mode       = NRF_SAADC_MODE_SINGLE_ENDED;
    channel_config0.burst      = NRF_SAADC_BURST_DISABLED;
    channel_config0.pin_p      = NRF_SAADC_INPUT_AIN1;       // o2 sensor
    channel_config0.pin_n      = NRF_SAADC_INPUT_DISABLED;
    channel_config0.reference  = NRF_SAADC_REFERENCE_INTERNAL;
///
    channel_config1.resistor_p = NRF_SAADC_RESISTOR_DISABLED;
    channel_config1.resistor_n = NRF_SAADC_RESISTOR_DISABLED;
    channel_config1.gain       = NRF_SAADC_GAIN1_6; //1_6
    channel_config1.acq_time   = NRF_SAADC_ACQTIME_10US;
    channel_config1.mode       = NRF_SAADC_MODE_SINGLE_ENDED;
    channel_config1.burst      = NRF_SAADC_BURST_DISABLED;
    channel_config1.pin_p      = NRF_SAADC_INPUT_AIN4;       // bat volt divider
    channel_config1.pin_n      = NRF_SAADC_INPUT_DISABLED;
    channel_config1.reference  = NRF_SAADC_REFERENCE_INTERNAL;

    err_code = nrf_drv_saadc_init(&saadc_config, saadc_callback);
    APP_ERROR_CHECK(err_code);
   // NRF_LOG_INFO("SAADC init");

    err_code = nrf_drv_saadc_channel_init(0, &channel_config0);
    APP_ERROR_CHECK(err_code);
 //   NRF_LOG_INFO("SAADC channel0 init");

    err_code = nrf_drv_saadc_channel_init(1, &channel_config1);
    APP_ERROR_CHECK(err_code);
//    NRF_LOG_INFO("SAADC channel1 init");

    err_code = nrf_drv_saadc_buffer_convert(m_buffer_pool[0], SAMPLES_IN_BUFFER);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(m_buffer_pool[1], SAMPLES_IN_BUFFER);
    APP_ERROR_CHECK(err_code);
  //  NRF_LOG_INFO("SAADC buffer init");

}

/*
static uint8_t m_beacon_info[APP_BEACON_INFO_LENGTH] =
{
    APP_DEVICE_TYPE,     // Manufacturer specific information. Specifies the device type in this
                         // implementation.
    APP_ADV_DATA_LENGTH, // Manufacturer specific information. Specifies the length of the
                         // manufacturer specific data in this implementation.
    APP_BEACON_UUID,     // 128 bit UUID value.
    APP_MAJOR_VALUE,     // Major arbitrary value that can be used to distinguish between Beacons.
    APP_MINOR_VALUE,     // Minor arbitrary value that can be used to distinguish between Beacons.
    APP_MEASURED_RSSI    // Manufacturer specific information. The Beacon's measured TX power in
                         // this implementation.
};
*/

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
}

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

int main(void)
{
    uint32_t err_code = NRF_SUCCESS;

    log_init();

    timers_init();
    leds_init();
    power_management_init();
    ble_stack_init();

    create_timers();
    err_code = app_timer_start(m_repeated_timer_id, APP_TIMER_TICKS(15000), NULL);

 //   NRF_LOG_INFO("Beacon example started.");
 //   NRF_POWER->DCDCEN = 1;                           //Enabling the DCDC converter for lower current consumption
    // do ble stack before crypto
    saadc_init();
    saadc_sampling_event_init();
    saadc_sampling_event_enable();
 //   NRF_LOG_INFO("SAADC HAL simple example started.");

  //  NRF_LOG_INFO("Initializing nrf_crypto.");
    err_code = nrf_crypto_init();
    APP_ERROR_CHECK(err_code);
  //  NRF_LOG_INFO("Initialized nrf_crypto.");
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

    for (;;)
    {
        idle_state_handle();
        nrf_pwr_mgmt_run();
        NRF_LOG_FLUSH();
    }
}

/**
 *@}
 **/
