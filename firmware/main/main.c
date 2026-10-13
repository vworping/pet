#include "waveshare_rgb_lcd_port.h"
#include "pet_ui.h"
#include "pet_services.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "pet_rfid.h"

void app_main(void)
{
    ESP_LOGI("pet", "[pet.] %s connected badge boot; 1024x600 landscape", esp_app_get_description()->version);
    ESP_ERROR_CHECK(waveshare_esp32_s3_rgb_lcd_init());
    pet_services_init();
    if (lvgl_port_lock(-1)) {
        pet_ui_init();
        lvgl_port_unlock();
    }
    ESP_LOGI("pet", "UI ready; v6 live Wi-Fi, phone introductions, backlight sleep.");
    pet_rfid_start();
#ifdef CONFIG_PET_HELLO_SOAK_TEST
    extern void pet_hello_soak(void);
    pet_hello_soak();
#endif
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        ESP_LOGI("pet", "alive; free heap=%lu", (unsigned long)esp_get_free_heap_size());
    }
}
