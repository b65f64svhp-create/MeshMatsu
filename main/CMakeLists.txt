idf_build_get_property(idf_target IDF_TARGET)
if("${idf_target}" STREQUAL "esp32p4")
list(APPEND extra_requires esp-hosted-tanmatsu)
endif()


idf_component_register(
	SRCS
		"main.c"
        "first_use.c"
        "third_party/qrcodegen.c"
		"packet_runner.c"
		"packet_scores.c"
        "config_backup.c"
        "config_backup_store.c"
        "config_local.c"
        "key_backup.c"
        "key_backup_store.c"
		"message_search.c"
        "chat_actions.c"
        "draft_store.c"
        "message_navigation.c"
        "range_export.c"
        "settings_menu.c"
		"sound_settings.c"
		"sound_player.c"
		"map_view.c"
		"location_utils.c"
        "units.c"
		"telemetry_format.c"
        "telemetry_store.c"
        "telemetry_csv.c"
        "telemetry_export.c"
        "message_export.c"
        "message_archive.c"
		"map_archive.c"
		"map_cache.c"
		"map_tiles.c"
		"radio_link.c"
		"node_details.c"
		"node_browser.c"
		"text_settings.c"
		"notification_time.c"
		"role_options.c"
		"brand_logo.c"
		"emoji_text.c"
		"emoji_picker.c"
		"composer_view.c"
		"display_settings.c"
		"power_settings.c"
		"backlight.c"
		"backlight_curve.c"
	PRIV_REQUIRES
		esp_lcd
        mbedtls
		fatfs
		sdmmc
		esp_driver_sdmmc
		esp_rom
		nvs_flash
		driver
		bt
		badge-bsp
		tanmatsu-wifi
		${extra_requires}
	INCLUDE_DIRS
		"."
)

idf_build_set_property(COMPILE_OPTIONS "-Wno-error=unused-variable" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=unused-const-variable" APPEND)
