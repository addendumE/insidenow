#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "dmx.h"

static const char *TAG = "DMX";

static QueueHandle_t uartQueue;
static QueueHandle_t dmxQueue;
static uart_port_t uartId;
static size_t dmxChannels;
static volatile uint8_t *dmxData;

void dmx_rx_task(void *arg)
{
    static int dmxIndex = 0;
    uart_event_t event;
    uint8_t byte;
    ESP_LOGI(TAG, "READY");
    while (1) {
        if (xQueueReceive(uartQueue, &event, portMAX_DELAY)) {

            switch (event.type) {

            case UART_BREAK:
                // Inizio nuovo frame DMX
                dmxIndex = 0;
                break;

            case UART_DATA:
                while (uart_read_bytes(uartId, &byte, 1, 0)) {
                    if (dmxIndex <= dmxChannels) {
                        dmxData[dmxIndex++] = byte;
                    }
                }

                if (dmxIndex > dmxChannels) {
                    xQueueSend(dmxQueue, (void *)&dmxData[1], 0);
                }
                break;

            case UART_FIFO_OVF:
            case UART_BUFFER_FULL:
                dmxIndex = 0;
                uart_flush_input(uartId);
                xQueueReset(uartQueue);
                break;

            default:
                break;
            }
        }
    }
}

bool dmxGet( uint8_t * dest)
{
    // 1 frame ogni 20 ms circa, dopo 50ms sync perso
    return xQueueReceive(dmxQueue, (void *)dest, 50 / portTICK_PERIOD_MS) == pdTRUE;
}

bool dmxSet( uint8_t * dest)
{
    //inietta un frame nella coda (debug)
    return xQueueSend(dmxQueue, (void *)dest, 0) == pdTRUE;
}


void dmxInit(uart_port_t _uartId,gpio_num_t _rxPin, size_t _dmxChannels)
{
    uartId = _uartId;
    dmxChannels = _dmxChannels;
    dmxQueue = xQueueCreate(1, dmxChannels);//1 frame in coda
    dmxData = malloc (dmxChannels + 1); // slot 0 = start code

    uart_config_t uart_config = {
        .baud_rate = 250000,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_RTC
    };
    
    ESP_ERROR_CHECK(uart_driver_install(uartId, 1024 * 2, 0, 20, &uartQueue, ESP_INTR_FLAG_IRAM));

    ESP_ERROR_CHECK(uart_param_config(uartId, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uartId, UART_PIN_NO_CHANGE, _rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    xTaskCreate(
        dmx_rx_task,
        "dmx_rx_task",
        4096,
        NULL,
        10,
        NULL
    );
    // Abilita rilevamento BREAK
    ESP_ERROR_CHECK(uart_enable_intr_mask(uartId, UART_BRK_DET_INT_ENA));

}
