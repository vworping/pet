# Keep fonts explicit so an existing build picks up new generated assets.
set(PET_FONTS fonts/pet_archivo_192.c fonts/pet_archivo_104.c fonts/pet_archivo_72.c fonts/pet_archivo_32.c
    fonts/pet_dm_18.c fonts/pet_dm_24.c fonts/pet_dm_28.c fonts/pet_dm_36.c
    fonts/pet_greeting_96.c fonts/pet_greeting_128.c)
file(GLOB PET_IMAGES "images/*.c")
idf_component_register(
    SRCS "main.c" "pet_rfid.c" "pet_cloud.c" "pet_ndef.c" "pet_ui.c" "pet_constellation.c" "pet_services.c" "pet_enterprise.c" "pet_protocol.c" "pet_socket.c" "pet_qr.c" "pet_portal.c" "board/lvgl_port.c" "board/waveshare_rgb_lcd_port.c" ${PET_FONTS} ${PET_IMAGES}
    INCLUDE_DIRS "." "board" "fonts" "images"
    REQUIRES esp_app_format wpa_supplicant esp_lcd driver esp_timer esp_wifi esp_netif esp_event nvs_flash esp_http_server esp_http_client mbedtls lwip json
    EMBED_FILES "guest.html")
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
if(CONFIG_PET_HELLO_SOAK_TEST)
    target_sources(${COMPONENT_LIB} PRIVATE "hello_soak.c")
endif()

# Institution-provided trust settings for PEAP/TTLS networks such as eduroam.
if(NOT CONFIG_PET_UMB_EDUROAM AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/enterprise_ca.pem")
    if(NOT CONFIG_PET_ENTERPRISE_DOMAIN)
        message(FATAL_ERROR "Set PET_ENTERPRISE_DOMAIN to your institution's authentication server name")
    endif()
    target_add_binary_data(${COMPONENT_LIB} "enterprise_ca.pem" TEXT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PET_ENTERPRISE_CA=1)
endif()
