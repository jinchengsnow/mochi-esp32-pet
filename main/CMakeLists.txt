if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    string(TIMESTAMP PET_BUILD_EPOCH "%s" UTC)
    configure_file("${CMAKE_CURRENT_LIST_DIR}/pet_clock_build.h.in" "${CMAKE_CURRENT_BINARY_DIR}/pet_clock_build.h" @ONLY)
endif()
idf_component_register(SRCS "main.c" "pet_model.c" "pet_routine.c" "pet_ui.c" "pet_font.c" "pet_save.c" "pet_store.c" "pet_battery.c" "pet_device.c" "pet_power_key.c" "pet_boot_key.c" "pet_touch.c" "pet_audio.c" "pet_sound.c"
                       INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}" REQUIRES esp_timer esp_hw_support esp_app_format nvs_flash esp_driver_i2s esp_driver_gpio esp_codec_dev)
