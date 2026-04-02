/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "nvs_flash.h"
#include "dmx.h"
#include "now.h"
#include "nvs.h"
#include "console.h"
#include "led_strip.h"

//#define DEBUG_VISIVO 1

static const char *TAG = "MAIN";
#define MAX_GATEWAY_PAYLOAD 250

#define NOW_CHANNEL 6
#define MAX_NODES   10
#define NUM_LEDS    12

// GATEWAY MODE CONFIGURATION
#define UART_RX_PIN     GPIO_NUM_9
#define LED_STS_PIN     GPIO_NUM_7
#define LED_CTRL_PIN    GPIO_NUM_7
// ATTENZIONE: LED_POWER_PIN era GPIO3, ora GPIO3 è usato come NODE_EXTPWR_GPIO (input).
// Spostare il pin di alimentazione della LED strip su un altro GPIO libero.
#define LED_POWER_PIN   GPIO_NUM_10

// NODE MODE CONFIGURATION

// --- Gestione risparmio energetico e batteria (solo modalità nodo) ---

// GPIO tasto wake-up/spegnimento (active-low: pull-up esterno, GND quando premuto)
#define NODE_WAKEUP_GPIO                    GPIO_NUM_4

// GPIO segnale alimentazione esterna (active-high: HIGH = collegato all'alimentazione/carica)
#define NODE_EXTPWR_GPIO                    GPIO_NUM_3

// Timeout dall'avvio senza aver mai ricevuto dati → deep sleep (ms)
#define TIMEOUT_SPEGNIMENTO_AVVIO           (5 * 60 * 1000)

// Tempo senza dati (dopo averne ricevuti) prima di iniziare il blink arancio (ms)
#define TIMEOUT_SEGNALE_PERSO               (2000)

// Tempo totale in blink arancio prima di andare in deep sleep (ms)
#define TIMEOUT_SPEGNIMENTO_SEGNALE_PERSO   (5 * 60 * 1000)

// Sequenza LED rossi prima del deep sleep: numero di lampeggi e semi-periodo (ms)
#define BLINK_SPEGNIMENTO_NUM               5
#define BLINK_SPEGNIMENTO_SEMIPERIODO_MS    200

// Semi-periodo del blink arancio durante perdita segnale (ms)
#define BLINK_ARANCIO_SEMIPERIODO_MS        400

// Durata pressione lunga tasto per spegnimento manuale (ms)
#define LONG_PRESS_MS                       3000

// Lampeggio verde all'avvio: numero di lampeggi e semi-periodo (ms)
#define BLINK_AVVIO_NUM                     3
#define BLINK_AVVIO_SEMIPERIODO_MS          200

// --- ADC batteria ---
// GPIO2 = ADC1_CH2; partitore con due 4.7k → Vbat/2 entra in ADC
#define BATT_ADC_CHANNEL                    ADC_CHANNEL_2
#define BATT_ADC_ATTEN                      ADC_ATTEN_DB_12
#define BATT_ADC_SAMPLES                    4       // campioni da mediare per lettura
#define BATT_VOLTAGE_MIN_MV                 3200    // mV batteria scarica (Vbat)
#define BATT_VOLTAGE_MAX_MV                 4200    // mV batteria carica  (Vbat)
#define BATT_DIVIDER_RATIO                  2       // Vbat = Vadc * BATT_DIVIDER_RATIO

// --- Indicatore stato carica all'avvio (solo su batteria) ---
#define BATT_STARTUP_LED_IDX                0       // indice LED che mostra la carica
#define BATT_STARTUP_SHOW_MS                2000    // durata visualizzazione (ms)

// --- LED breathing durante la carica (solo con alimentazione esterna) ---
#define CHARGE_LED_IDX                      0       // indice LED riservato al breathing
#define CHARGE_BREATHING_PERIOD_MS          3000    // periodo del breathing (ms)
#define CHARGE_BREATHING_MAX_BRIGHTNESS     220     // luminosità massima (0-255)
#define CHARGE_UPDATE_PERIOD_MS             20      // periodo aggiornamento breathing (ms)

// --- Fine defines ---


int nodeAddress; // se nodeAddress < 0 → modalità gateway
static led_strip_handle_t pStrip_a;
static volatile bool isCharging = false;
static adc_oneshot_unit_handle_t adcHandle;
static adc_cali_handle_t adcCaliHandle;

static int consoleAddress(int argc, char **argv);
static int consoleReboot(int argc, char **argv);
static int consoleTx(int argc, char **argv);
static int consoleLed(int argc, char **argv);
static void gatewayTask(void *arg);
static void nodeTask(void *arg);
static void buttonTask(void *arg);
static void chargingTask(void *arg);
static void led_strip_set_all_rgb(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2);
static void node_led_clear(void);
static void node_go_to_sleep(void);
static int   batt_read_mv(void);
static float batt_get_level(void);
static void  batt_level_to_rgb(float level, uint8_t *r, uint8_t *g, uint8_t *b);


static const esp_console_cmd_t cmd_tx = {
	 .command = "tx",
	 .help = NULL,
	 .hint = NULL,
	 .func = &consoleTx,
	 .argtable = NULL,
	 .func_w_context = NULL,
	 .context = NULL
};

static const esp_console_cmd_t cmd_address = {
	 .command = "address",
	 .help = NULL,
	 .hint = NULL,
	 .func = &consoleAddress,
	 .argtable = NULL,
	 .func_w_context = NULL,
	 .context = NULL
};

static const esp_console_cmd_t cmd_led = {
     .command = "led",
     .help = NULL,
     .hint = NULL,
     .func = &consoleLed,
     .argtable = NULL,
     .func_w_context = NULL,
     .context = NULL
};

static const esp_console_cmd_t cmd_reboot = {
	 .command = "reboot",
	 .help = NULL,
	 .hint = NULL,
	 .func = &consoleReboot,
	 .argtable = NULL,
	 .func_w_context = NULL,
	 .context = NULL
};

int consoleTx(int argc, char **argv)
{
    if (argc == 2)
    {
        size_t len = strlen(argv[1])/2;
        if (len > MAX_PAYLOAD_SIZE) len = MAX_PAYLOAD_SIZE;
        uint8_t *data = malloc(len);
        char *pos = argv[1];
        for (size_t count = 0; count < len; count++) {
            if (sscanf(pos, "%2hhx", data+count) != 1)
            {
                ESP_LOGE(TAG,"wrong payload data");
                free(data);
                return -1;
            }
            pos += 2;
        }
        nowSend(data, len);
        free(data);
    }
    return 0;
}

int consoleAddress(int argc, char **argv)
{
    if (argc == 2) {
        int addr;
        if (sscanf(argv[1],"%d",&addr) == 1) {
            if (addr < MAX_NODES) {
                nvsSetNodeAddress(addr);
            }
            else {
                ESP_LOGE(TAG,"max address is %d",MAX_NODES);
                return -1;
            }
        }
        else {
            ESP_LOGE(TAG,"wrong address format");
            return -1;
        }
    }
    else {
        printf ("node address is: %d\n",nvsGetNodeAddress());
    }
    return 0;
}

int consoleReboot(int argc, char **argv)
{
    esp_restart();
    return 0;
}

int consoleLed(int argc, char **argv)
{
    if (argc == 4) {
        int r,g,b;
        if (sscanf(argv[1],"%d",&r) == 1 && sscanf(argv[2],"%d",&g) == 1 && sscanf(argv[3],"%d",&b) == 1) {
            led_strip_set_all_rgb(r,g,b,0,0,0);
        }
        else {
            ESP_LOGE(TAG,"wrong color format");
            return -1;
        }
    }
    else {
        ESP_LOGE(TAG,"usage: led R G B");
        return -1;
    }
    return 0;
}

void led_toggle()
{
    static bool level = false;
    gpio_set_level(LED_STS_PIN, level);
    level = !level;
}

// MAIN TASK FOR GATEWAY MODE
void gatewayTask(void *arg)
{
    uint32_t oldCrc, frmCounter = 0, failCounter = 0;
    unsigned char dmxData[DMX_DATA_SIZE];
    ESP_LOGI(TAG, "MAIN GATEWAY TASK READY");

    while(1) {
        if (dmxGet(dmxData)) {
            led_toggle();
            uint32_t newCrc = esp_rom_crc32_le(0, dmxData, sizeof(dmxData));
            if ((frmCounter % 15) == 0 || oldCrc != newCrc) {
                ESP_LOG_BUFFER_HEX_LEVEL("TX", dmxData,16, ESP_LOG_INFO);
                nowSend(dmxData,MAX_GATEWAY_PAYLOAD);
                oldCrc = newCrc;
            }
            frmCounter++;
            failCounter = 0;
        }
        else {
            failCounter++;
            frmCounter = 0;
            if (failCounter % 25 == 0) {
               led_toggle();
            }
        }
    }
}

/*
 * Imposta tutti i LED della strip.
 * Quando isCharging è true, il LED CHARGE_LED_IDX è saltato perché
 * gestito esclusivamente da chargingTask.
 */
void led_strip_set_all_rgb(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2)
{
    gpio_set_level(LED_POWER_PIN, 1);
    for (int i = 0; i < NUM_LEDS/2; i++) {
        if (isCharging && i == CHARGE_LED_IDX) continue;
        ESP_ERROR_CHECK(led_strip_set_pixel(pStrip_a, i, r1, g1, b1));
    }
    for (int i = NUM_LEDS/2; i < NUM_LEDS; i++) {
        if (isCharging && i == CHARGE_LED_IDX) continue;
        ESP_ERROR_CHECK(led_strip_set_pixel(pStrip_a, i, r2, g2, b2));
    }
    ESP_ERROR_CHECK(led_strip_refresh(pStrip_a));
}

/*
 * Spegne tutti i LED tranne CHARGE_LED_IDX quando in carica.
 */
static void node_led_clear(void)
{
    if (isCharging) {
        for (int i = 0; i < NUM_LEDS; i++) {
            if (i == CHARGE_LED_IDX) continue;
            led_strip_set_pixel(pStrip_a, i, 0, 0, 0);
        }
        led_strip_refresh(pStrip_a);
    } else {
        led_strip_clear(pStrip_a);
    }
}

// ---------------------------------------------------------------------------
// Batteria
// ---------------------------------------------------------------------------

/*
 * Legge la tensione di batteria in mV (Vbat, già moltiplicata per il divider).
 * Usa la calibrazione ADC per ottenere mV precisi.
 */
static int batt_read_mv(void)
{
    int sum = 0, raw = 0;
    for (int i = 0; i < BATT_ADC_SAMPLES; i++) {
        adc_oneshot_read(adcHandle, BATT_ADC_CHANNEL, &raw);
        sum += raw;
    }
    raw = sum / BATT_ADC_SAMPLES;
    int mv_adc = 0;
    adc_cali_raw_to_voltage(adcCaliHandle, raw, &mv_adc);
    return mv_adc * BATT_DIVIDER_RATIO;
}

/*
 * Restituisce il livello di carica normalizzato (0.0 = scarica, 1.0 = carica).
 */
static float batt_get_level(void)
{
    int mv = batt_read_mv();
    float level = (float)(mv - BATT_VOLTAGE_MIN_MV) /
                  (float)(BATT_VOLTAGE_MAX_MV - BATT_VOLTAGE_MIN_MV);
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    return level;
}

/*
 * Mappa livello di carica (0.0-1.0) su colore rosso→verde.
 */
static void batt_level_to_rgb(float level, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (uint8_t)(255.0f * (1.0f - level));
    *g = (uint8_t)(255.0f * level);
    *b = 0;
}

// ---------------------------------------------------------------------------
// Deep sleep
// ---------------------------------------------------------------------------

/*
 * Sequenza di lampeggio rosso + deep sleep.
 * Sorgenti di wakeup:
 *   - NODE_WAKEUP_GPIO (GPIO5, tasto) → LOW
 *   - NODE_EXTPWR_GPIO (GPIO3, alimentazione esterna) → HIGH
 * Non ritorna.
 */
static void node_go_to_sleep(void)
{
    ESP_LOGI(TAG, "Entering deep sleep...");

    // Lampeggio LED rossi
    isCharging = false; // forza scrittura su tutti i LED nella sequenza di spegnimento
    for (int i = 0; i < BLINK_SPEGNIMENTO_NUM; i++) {
        led_strip_set_all_rgb(255, 0, 0, 255, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(BLINK_SPEGNIMENTO_SEMIPERIODO_MS));
        led_strip_clear(pStrip_a);
        vTaskDelay(pdMS_TO_TICKS(BLINK_SPEGNIMENTO_SEMIPERIODO_MS));
    }

    gpio_set_level(LED_POWER_PIN, 0);

    // Aspetta rilascio tasto (ESP32-C3: wakeup level-triggered, non edge)
    while (gpio_get_level(NODE_WAKEUP_GPIO) == 0) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    vTaskDelay(pdMS_TO_TICKS(50)); // debounce

    // Configura sorgenti di wakeup
    ESP_ERROR_CHECK(esp_deep_sleep_enable_gpio_wakeup(
        1ULL << NODE_WAKEUP_GPIO, ESP_GPIO_WAKEUP_GPIO_LOW));   // tasto premuto
    ESP_ERROR_CHECK(esp_deep_sleep_enable_gpio_wakeup(
        1ULL << NODE_EXTPWR_GPIO, ESP_GPIO_WAKEUP_GPIO_HIGH));  // alimentazione esterna collegata

    esp_deep_sleep_start();
}

// ---------------------------------------------------------------------------
// Task nodo
// ---------------------------------------------------------------------------

void nodeTask(void *arg)
{
    typedef enum {
        NODE_PWR_WAITING_FIRST,
        NODE_PWR_ACTIVE,
        NODE_PWR_SIGNAL_LOST,
    } node_pwr_state_t;

    node_pwr_state_t pwrState   = NODE_PWR_WAITING_FIRST;
    TickType_t stateStartTick   = xTaskGetTickCount();
    TickType_t lastDataTick     = xTaskGetTickCount();
    TickType_t lastBlinkTick    = xTaskGetTickCount();
    bool blinkLedOn = false;

    ESP_LOGI(TAG, "MAIN NODE TASK READY WITH ADDRESS %d", nodeAddress);

    while(1)
    {
        t_payload pl;
        bool received = nowReceive(&pl); // attende al massimo 50ms

        if (received) {
            ESP_LOG_BUFFER_HEX_LEVEL("RECEIVE", pl.data, pl.len, ESP_LOG_INFO);

            if (nodeAddress >= 0) {
                size_t idx = nodeAddress * 3;
                if (pl.len >= idx + 6) {
                    uint8_t r1 = pl.data[idx + 0];
                    uint8_t g1 = pl.data[idx + 1];
                    uint8_t b1 = pl.data[idx + 2];
                    uint8_t r2 = pl.data[idx + 3];
                    uint8_t g2 = pl.data[idx + 4];
                    uint8_t b2 = pl.data[idx + 5];
                    ESP_LOGI(TAG, "Applying color R:%d G:%d B:%d + R:%d G:%d B:%d", r1, g1, b1, r2, g2, b2);
                    led_strip_set_all_rgb(r1, g1, b1, r2, g2, b2);
                }
                else {
                    ESP_LOGW(TAG, "payload too short for node %d (len=%d)", nodeAddress, pl.len);
                }
            }

            lastDataTick = xTaskGetTickCount();

            if (pwrState != NODE_PWR_ACTIVE) {
                ESP_LOGI(TAG, "Signal restored, switching to ACTIVE");
                pwrState = NODE_PWR_ACTIVE;
                stateStartTick = xTaskGetTickCount();
            }
        }

        // Con alimentazione esterna non si va in sleep per nessun motivo
        if (isCharging) continue;

        TickType_t now = xTaskGetTickCount();

        switch (pwrState) {

            case NODE_PWR_WAITING_FIRST:
                if ((now - stateStartTick) >= pdMS_TO_TICKS(TIMEOUT_SPEGNIMENTO_AVVIO)) {
                    ESP_LOGW(TAG, "No data within startup timeout, going to sleep");
                    node_go_to_sleep();
                }
                break;

            case NODE_PWR_ACTIVE:
                if ((now - lastDataTick) >= pdMS_TO_TICKS(TIMEOUT_SEGNALE_PERSO)) {
                    ESP_LOGW(TAG, "Signal lost, starting orange blink");
                    pwrState      = NODE_PWR_SIGNAL_LOST;
                    stateStartTick = xTaskGetTickCount();
                    lastBlinkTick  = xTaskGetTickCount();
                    blinkLedOn = false;
                }
                break;

            case NODE_PWR_SIGNAL_LOST:
                if ((now - stateStartTick) >= pdMS_TO_TICKS(TIMEOUT_SPEGNIMENTO_SEGNALE_PERSO)) {
                    ESP_LOGW(TAG, "Signal lost timeout expired, going to sleep");
                    node_go_to_sleep();
                }
                if ((now - lastBlinkTick) >= pdMS_TO_TICKS(BLINK_ARANCIO_SEMIPERIODO_MS)) {
                    lastBlinkTick = now;
                    blinkLedOn = !blinkLedOn;
                    if (blinkLedOn) {
                        led_strip_set_all_rgb(255, 80, 0, 255, 80, 0);
                    } else {
                        node_led_clear();
                    }
                }
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// Task carica (alimentazione esterna)
// ---------------------------------------------------------------------------

/*
 * Monitora NODE_EXTPWR_GPIO.
 * Quando HIGH (alimentazione esterna):
 *   - imposta isCharging = true
 *   - effetto breathing sul LED CHARGE_LED_IDX con colore rosso→verde in base alla carica
 * Quando LOW (a batteria):
 *   - imposta isCharging = false
 *   - spegne CHARGE_LED_IDX
 */
static void chargingTask(void *arg)
{
    ESP_LOGI(TAG, "Charging task ready on GPIO %d", NODE_EXTPWR_GPIO);
    TickType_t breathingStart = xTaskGetTickCount();

    while(1)
    {
        bool extPwr = (gpio_get_level(NODE_EXTPWR_GPIO) == 1);

        if (extPwr) {
            if (!isCharging) {
                ESP_LOGI(TAG, "External power detected, charging mode ON");
                isCharging = true;
                breathingStart = xTaskGetTickCount();
            }

            float level = batt_get_level();
            uint8_t r, g, b;
            batt_level_to_rgb(level, &r, &g, &b);

            // Breathing sinusoidale: 0 → max → 0 in un periodo
            float phase = (float)((xTaskGetTickCount() - breathingStart) %
                                   pdMS_TO_TICKS(CHARGE_BREATHING_PERIOD_MS)) /
                          (float)pdMS_TO_TICKS(CHARGE_BREATHING_PERIOD_MS);
            float brightness = (sinf(phase * 2.0f * (float)M_PI) + 1.0f) / 2.0f *
                               (CHARGE_BREATHING_MAX_BRIGHTNESS / 255.0f);

            led_strip_set_pixel(pStrip_a, CHARGE_LED_IDX,
                                (uint8_t)(r * brightness),
                                (uint8_t)(g * brightness),
                                (uint8_t)(b * brightness));
            led_strip_refresh(pStrip_a);
        }
        else {
            if (isCharging) {
                ESP_LOGI(TAG, "External power removed, charging mode OFF");
                isCharging = false;
                // Spegni il LED di carica
                led_strip_set_pixel(pStrip_a, CHARGE_LED_IDX, 0, 0, 0);
                led_strip_refresh(pStrip_a);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(CHARGE_UPDATE_PERIOD_MS));
    }
}

// ---------------------------------------------------------------------------
// Task tasto
// ---------------------------------------------------------------------------

static void buttonTask(void *arg)
{
    ESP_LOGI(TAG, "Button task ready on GPIO %d", NODE_WAKEUP_GPIO);

    while(1)
    {
        if (gpio_get_level(NODE_WAKEUP_GPIO) == 0) {
            TickType_t pressStart = xTaskGetTickCount();
            while (gpio_get_level(NODE_WAKEUP_GPIO) == 0) {
                vTaskDelay(pdMS_TO_TICKS(20));
                if ((xTaskGetTickCount() - pressStart) >= pdMS_TO_TICKS(LONG_PRESS_MS)) {
                    ESP_LOGW(TAG, "Long press detected, going to sleep");
                    node_go_to_sleep();
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ---------------------------------------------------------------------------
// Debug visivo
// ---------------------------------------------------------------------------

void visive_debug_task(void *arg)
{
    typedef enum {
        STATE_RED, STATE_GREEN, STATE_BLUE,
        STATE_RED_BLUE, STATE_GREEN_BLUE, STATE_WHITE_BLUE,
        NUM_STATES
    } led_state_t;

    led_state_t currentState = STATE_RED;
    ESP_LOGI(TAG, "Visual debug task started");

    while(1)
    {
        switch (currentState) {
            case STATE_RED:       led_strip_set_all_rgb(255,   0,   0, 255,   0,   0); break;
            case STATE_GREEN:     led_strip_set_all_rgb(  0, 255,   0,   0, 255,   0); break;
            case STATE_BLUE:      led_strip_set_all_rgb(  0,   0, 255,   0,   0, 255); break;
            case STATE_RED_BLUE:  led_strip_set_all_rgb(255,   0,   0,   0,   0, 255); break;
            case STATE_GREEN_BLUE:led_strip_set_all_rgb(  0, 255,   0,   0,   0, 255); break;
            case STATE_WHITE_BLUE:led_strip_set_all_rgb(255, 255, 255,   0,   0, 255); break;
            default: currentState = STATE_RED; break;
        }
        currentState = (currentState + 1) % NUM_STATES;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void gatewayInit()
{
    dmxInit(UART_NUM_1, UART_RX_PIN);
    gpio_reset_pin(LED_STS_PIN);
    gpio_set_direction(LED_STS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_STS_PIN, true);
    nowInit(NOW_CHANNEL, false, false);
    xTaskCreate(gatewayTask, "gatewayTask", 4096, NULL, 10, NULL);
}

void nodeInit()
{
    nowInit(NOW_CHANNEL, false, true);

    // Alimentazione LED strip
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << LED_POWER_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_POWER_PIN, 1);

    // LED strip
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_CTRL_PIN,
        .max_leds = NUM_LEDS,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &pStrip_a));

    // GPIO tasto (input, pull-up esterno, active-low)
    gpio_config_t btn_conf = {
        .pin_bit_mask = 1ULL << NODE_WAKEUP_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,  // pull-up esterno
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_conf);

    // GPIO alimentazione esterna (input, pull-down: LOW quando non collegato)
    gpio_config_t extpwr_conf = {
        .pin_bit_mask = 1ULL << NODE_EXTPWR_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&extpwr_conf);

    // ADC batteria
    adc_oneshot_unit_init_cfg_t adc_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_cfg, &adcHandle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = BATT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adcHandle, BATT_ADC_CHANNEL, &chan_cfg));

    // Calibrazione ADC: su ESP32-C3 è supportato solo lo schema curve fitting
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = ADC_UNIT_1,
        .chan     = BATT_ADC_CHANNEL,
        .atten    = BATT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_cfg, &adcCaliHandle));

    // --- Lampeggio verde di avvio ---
    for (int i = 0; i < BLINK_AVVIO_NUM; i++) {
        led_strip_set_all_rgb(0, 255, 0, 0, 255, 0);
        vTaskDelay(pdMS_TO_TICKS(BLINK_AVVIO_SEMIPERIODO_MS));
        led_strip_clear(pStrip_a);
        vTaskDelay(pdMS_TO_TICKS(BLINK_AVVIO_SEMIPERIODO_MS));
    }

    // --- Indicatore batteria all'avvio (solo se a batteria) ---
    if (gpio_get_level(NODE_EXTPWR_GPIO) == 0) {
        float level = batt_get_level();
        uint8_t r, g, b;
        batt_level_to_rgb(level, &r, &g, &b);
        ESP_LOGI(TAG, "Battery level: %.0f%% (%d mV)", level * 100.0f, batt_read_mv());
        led_strip_set_pixel(pStrip_a, BATT_STARTUP_LED_IDX, r, g, b);
        led_strip_refresh(pStrip_a);
        vTaskDelay(pdMS_TO_TICKS(BATT_STARTUP_SHOW_MS));
        led_strip_clear(pStrip_a);
    }

    xTaskCreate(nodeTask,     "nodeTask",     4096, NULL, 10, NULL);
    xTaskCreate(buttonTask,   "buttonTask",   2048, NULL,  9, NULL);
    xTaskCreate(chargingTask, "chargingTask", 3072, NULL,  9, NULL);
}

void visive_debug_init()
{
    xTaskCreate(visive_debug_task, "visive_debug_task", 4096, NULL, 10, NULL);
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    consoleInit();
    consoleRegister(&cmd_tx);
    consoleRegister(&cmd_reboot);
    consoleRegister(&cmd_address);
    nodeAddress = nvsGetNodeAddress();
    if (nodeAddress < 0) {
        gatewayInit();
        ESP_LOGI(TAG, "GATEWAY READY TO GO");
    }
    else {
        consoleRegister(&cmd_led);
        nodeInit();
        ESP_LOGI(TAG, "NODE READY TO GO ON ADDRESS: %d", nodeAddress);
        #ifdef DEBUG_VISIVO
            visive_debug_init();
        #endif
    }
}
