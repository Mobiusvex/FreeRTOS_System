SET(SOURCES components/ui_comp_slider4.c
    components/ui_comp.c
    screens/ui_ScreenWeather.c
    screens/ui_ScreenMusic.c
    screens/ui_ScreenMain.c
    screens/ui_ScreenSet.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_1609717271.c
    images/ui_img_rain_png.c
    images/ui_img_city_png.c
    images/ui_img_humi_png.c
    images/ui_img_airpressure_png.c
    images/ui_img_wind_png.c
    images/ui_img_2084818829.c
    images/ui_img_1046395525.c
    images/ui_img_wifi_png.c
    images/ui_img_1413029994.c
    images/ui_img_1466755499.c
    fonts/ui_font_siyuanMedium24_2.c
    fonts/ui_font_siyuanMedium44_4.c
    fonts/ui_font_siyuanMedium16_4.c)

add_library(ui ${SOURCES})
