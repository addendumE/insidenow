#include "console.h"


void consoleRegister(const esp_console_cmd_t *handler)
{
    ESP_ERROR_CHECK(esp_console_cmd_register(handler));
}

void consoleInit(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "DMX>";
    repl_config.max_cmdline_length = 128;
    esp_console_register_help_command();
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t dev_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&dev_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
#else
#error "Solo USB_SERIAL_JTAG è abilitato per la console. Abilita UART in menuconfig se vuoi usare la REPL UART."
#endif
}
