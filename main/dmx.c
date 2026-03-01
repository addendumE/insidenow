#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "dmx.h"

static const char *TAG = "DMX";

static QueueHandle_t uartQueue;
static QueueHandle_t dmxQueue;
static uart_port_t uartId;

static uint8_t dmxData[DMX_DATA_SIZE];

void dmx_rx_task(void *arg)
{
    uart_event_t event;
    size_t to_read;
    ESP_LOGI(TAG, "READY");
    while (1) {
        if (xQueueReceive(uartQueue, &event, portMAX_DELAY)) {
            switch (event.type) {
            case UART_BREAK:
                if (uart_get_buffered_data_len(uartId, &to_read) == 0) {
                    if (to_read == sizeof(dmxData)+1) {
                        uart_read_bytes(uartId, dmxData, 1,0);//skip first byte
                        uart_read_bytes(uartId, dmxData, sizeof(dmxData),0);
                        xQueueSend(dmxQueue, &dmxData, 0);
                    }
                    uart_flush_input(uartId);
                }
                else {
                    ESP_LOGE(TAG,"uart_get_buffered_data_len failed");
                }
                break;
            case UART_FIFO_OVF:
            case UART_BUFFER_FULL:
                ESP_LOGE(TAG,"uart fifo overflow");
                uart_flush_input(uartId);
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
    return xQueueReceive(dmxQueue, dest, 50 / portTICK_PERIOD_MS) == pdTRUE;
}

void dmxInit(uart_port_t _uartId,gpio_num_t _rxPin)
{
    uartId = _uartId;
    dmxQueue = xQueueCreate(1, sizeof(dmxData));//1 frame in coda
    uart_config_t uart_config = {
        .baud_rate = 250000,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB
    };
    
    ESP_ERROR_CHECK(uart_driver_install(uartId, 1024 * 2, 0, 20, &uartQueue, ESP_INTR_FLAG_IRAM));
    ESP_ERROR_CHECK(uart_param_config(uartId, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uartId, UART_PIN_NO_CHANGE, _rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_enable_intr_mask(uartId, UART_BRK_DET_INT_ENA | UART_FRM_ERR_INT_ENA));

    xTaskCreate(
        dmx_rx_task,
        "dmx_rx_task",
        4096,
        NULL,
        configMAX_PRIORITIES - 1,
        NULL
    );
}
