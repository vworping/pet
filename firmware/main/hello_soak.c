/* Hardware regression fixture, compiled only with CONFIG_PET_HELLO_SOAK_TEST.
   Does not submit introductions, alter credentials or change persistent counts. */
#include "lvgl_port.h"
#include "pet_services.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include <assert.h>
#include <string.h>

static const char *TAG="hello_soak";
static lv_obj_t *find_label(lv_obj_t *obj,const char *text){
    if(lv_obj_check_type(obj,&lv_label_class)&&!strcmp(lv_label_get_text(obj),text))return obj;
    for(unsigned i=0;i<lv_obj_get_child_cnt(obj);i++){
        lv_obj_t *found=find_label(lv_obj_get_child(obj,i),text);
        if(found)return found;
    }
    return NULL;
}
static void click(void *text){
    lv_obj_t *label=find_label(lv_scr_act(),text);
    assert(label);
    lv_event_send(lv_obj_get_parent(label),LV_EVENT_CLICKED,NULL);
}
static void schedule_click(const char *text){
    assert(lvgl_port_lock(-1));
    assert(lv_async_call(click,(void*)text)==LV_RES_OK);
    lvgl_port_unlock();
}
static void screen_contains(const char *text){
    assert(lvgl_port_lock(-1));
    assert(find_label(lv_scr_act(),text));
    lvgl_port_unlock();
}
void pet_hello_soak(void){
    static pet_status_t status;
    vTaskDelay(pdMS_TO_TICKS(3000));
    for(unsigned cycle=1;cycle<=10;cycle++){
        ESP_LOGI(TAG,"Opening Hello, cycle %u/10",cycle);
        schedule_click("Hello");
        vTaskDelay(pdMS_TO_TICKS(4000));
        pet_services_snapshot(&status);
        assert(status.hello_active&&!status.hello_starting);
        wifi_mode_t mode;ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));
        assert(mode==WIFI_MODE_APSTA);
        screen_contains("Join me.");
        assert(heap_caps_check_integrity_all(true));
        schedule_click("Home");
        vTaskDelay(pdMS_TO_TICKS(1500));
        pet_services_snapshot(&status);
        assert(!status.hello_active&&!status.hello_starting);
        ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));assert(mode==WIFI_MODE_STA);
        screen_contains("ANDRE\nWEISS.");
        assert(heap_caps_check_integrity_all(true));
        ESP_LOGI(TAG,"PASS cycle %u/10; free heap=%lu",cycle,(unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT));
    }
    ESP_LOGI(TAG,"PASS: ten Hello button/QR/AP/DNS/cancel cycles; heap intact");
}
