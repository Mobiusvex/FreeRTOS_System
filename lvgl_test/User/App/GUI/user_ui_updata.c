#include "user_ui_updata.h"
#include "ui.h"
#include "sys_data.h"
#include "user_ui_popup.h"

extern ThresholdType_t threshold_type;
extern const ThresholdData_t thresholds_range[THRESHOLD_TYPE_NUM];
extern char WeatherCity[12];

void user_ui_updata(SYS_DataEventType_t event, const SystemGlobalData_t *p_data) {
    switch (event) {
    case SYS_WIFI_UPDATE:
        if (p_data->wifi_status == WIFI_STATE_CONNECTED) {
            lv_obj_set_style_bg_color(ui_ImageWifi, lv_color_hex(0x6FD4ED), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_bg_color(ui_ImageWifi, lv_color_hex(0x6E7172), LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        lv_obj_set_style_bg_opa(ui_ImageWifi, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    /* ----- 时间相关更新 ----- */
    case SYS_TIME_UPDATE:
        // 更新主屏幕的时间、日期、星期（如有）
        lv_label_set_text_fmt(ui_LabelTime, "%02d:%02d", p_data->hour, p_data->minute);
        break;
    case SYS_TIME_SECOND_UPDATE:
        lv_label_set_text_fmt(ui_LabelSecond, "%02d", p_data->second);
        break;
    case SYS_DATE_UPDATE:
        lv_label_set_text_fmt(ui_LabelDate, "%04d-%02d-%02d", p_data->year, p_data->month, p_data->day);
        const char *weekday[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
        lv_label_set_text(ui_LabelWeek, weekday[p_data->weekday > 7 ? 6 : p_data->weekday - 1]);
        break;
    /* ----- 环境数据更新（主屏幕） ----- */
    case SYS_ENV_UPDATE:
        lv_label_set_text_fmt(ui_LabelTemp, "%.1f", (float)p_data->temperature / 10.0f);
        lv_label_set_text_fmt(ui_LabelHumi, "%.1f", p_data->humidity / 10.0f);
        break;

    /* ----- 姿态角更新（主屏幕） ----- */
    case SYS_ANGLE_UPDATE:
        lv_label_set_text_fmt(ui_LabelPitch, "%.1f", p_data->pitch / 10.0f);
        lv_label_set_text_fmt(ui_LabelRoll, "%.1f", p_data->roll / 10.0f);
        lv_label_set_text_fmt(ui_LabelYaw, "%.1f", p_data->yaw / 10.0f);
        break;
    case SYS_WEATHER_UPDATE: {
        uint8_t code = p_data->weather_code;

        switch (code) {
        case WEATHER_CODE_SUNNY:
            lv_label_set_text(ui_LabelWeather, "Sunny");
            lv_img_set_src(ui_ImageWeather, &ui_img_sunny_png);
            break;
        case WEATHER_CODE_CLOUDY:
            lv_label_set_text(ui_LabelWeather, "Cloudy");
            lv_img_set_src(ui_ImageWeather, &ui_img_cloudy_png);
            break;
        case WEATHER_CODE_RAINY:
            lv_label_set_text(ui_LabelWeather, "Rainy");
            lv_img_set_src(ui_ImageWeather, &ui_img_rain_png);
            break;
        case WEATHER_CODE_OVERCAST:
            lv_label_set_text(ui_LabelWeather, "Overcast");
            lv_img_set_src(ui_ImageWeather, &ui_img_overcast_png);
            break;
        case WEATHER_CODE_SNOWY:
            lv_label_set_text(ui_LabelWeather, "Snowy");
            lv_img_set_src(ui_ImageWeather, &ui_img_snow_png);
            break;
        } // switch (code)

        // 温度、湿度、气压、风力等级
        lv_label_set_text_fmt(ui_LabelWeatherTemp, "%d", p_data->weather_temp);
        lv_label_set_text_fmt(ui_LabelWeatherHumi, "%d%%", p_data->weather_humi);
        lv_label_set_text_fmt(ui_LabelWeatherPressure, "%d", p_data->weather_pressure);
        lv_label_set_text_fmt(ui_LabelWeatherWind, "%d", p_data->weather_wind);

        // 城市（若结构体无城市字段，可留空或从外部获取）
        lv_label_set_text(ui_LabelWeatherCity, p_data->weather_city);

        // 天气日期
        lv_label_set_text_fmt(ui_LabelWeatherdate, "%04d-%02d-%02d",
                              p_data->weather_update_year,
                              p_data->weather_update_month,
                              p_data->weather_update_day);

        break;
    }
    /* ---------- 阈值更新（设置屏幕） ---------- */
    case SYS_THRESHOLD_UPDATE:
        lv_label_set_text_fmt(ui_LabelMinValue, "%d", p_data->thresholds[threshold_type].threshold_low);
        lv_label_set_text_fmt(ui_LabelMaxValue, "%d", p_data->thresholds[threshold_type].threshold_high);
        lv_slider_set_value(ui_SliderThreshold, p_data->thresholds[threshold_type].threshold_high, LV_ANIM_OFF);
        lv_slider_set_left_value(ui_SliderThreshold, p_data->thresholds[threshold_type].threshold_low, LV_ANIM_OFF);
        // 滑块可以设置为低阈值或平均，这里设为低阈值
        lv_slider_set_range(ui_SliderThreshold, thresholds_range[threshold_type].threshold_low, thresholds_range[threshold_type].threshold_high);
        break;
    /* ---------- 音量更新 ---------- */
    case SYS_VOLUME_UPDATE:
        lv_slider_set_value(ui_SliderVolume, p_data->volume, LV_ANIM_OFF);
        break;

    case SYS_PAKET_UPDATE:
        if (p_data->paket_update_status == PAKET_UPDATE_DOWNLOAD) {
            lv_label_set_text(s_popup_label, "Downloading...");
            popup_clear_hidden();

        } else if (p_data->paket_update_status == PAKET_UPDATE_RESTART) {
            lv_label_set_text(s_popup_label, "Restarting...");
            popup_clear_hidden();
        } else {
            popup_hide();
        }
        // 第二行：更新进度条
        uint8_t v = (p_data->paket_update_progress > 100) ? 100 : p_data->paket_update_progress;
        lv_bar_set_value(s_popup_bar, v, LV_ANIM_OFF);
        break;

    /* ---------- 背景颜色更新 ---------- */
    case SYS_BACKGROUND_COLOR_UPDATE: {
        // background_color_code 为预设颜色索引，可根据需要映射
        lv_color_t color;
        switch (p_data->background_color_code) {
        case 0: color = lv_color_hex(0xFFFFFF); break; // 白色
        case 1: color = lv_color_hex(0xED9FD7); break; // 粉色
        case 2: color = lv_color_hex(0x9FC3ED); break; // 蓝色
        case 3: color = lv_color_hex(0xC196F5); break; // 紫色
        default:
            color = lv_color_hex(0xFFFFFF);
            break;
        }
        // 设置主屏幕背景（可继续添加其他屏幕）
        lv_obj_set_style_bg_color(ui_ScreenMain, color, LV_PART_MAIN);
        // 如需全局背景，可同时设置其他屏幕
        lv_obj_set_style_bg_color(ui_ScreenWeather, color, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ui_ScreenMusic, color, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ui_ScreenSet, color, LV_PART_MAIN);
        break;
    }

    default:
        break;
    }
}

/**
 * @brief 一次性刷新所有UI控件，使所有屏幕显示与最新系统数据同步
 * @param p_data 指向系统全局数据的指针
 */
void user_ui_refresh_all(const SystemGlobalData_t *p_data) {
    /* ========== 主屏幕 (ui_ScreenMain) ========== */
    lv_label_set_text_fmt(ui_LabelVersion, "version:%x.%x", (p_data->version >> 8), p_data->version & 0xFF);
    // 时间、日期、星期
    lv_label_set_text_fmt(ui_LabelTime, "%02d:%02d", p_data->hour, p_data->minute);
    lv_label_set_text_fmt(ui_LabelSecond, "%02d", p_data->second);
    lv_label_set_text_fmt(ui_LabelDate, "%04d-%02d-%02d", p_data->year, p_data->month, p_data->day);

    // 星期
    const char *weekday[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
    lv_label_set_text(ui_LabelWeek, weekday[p_data->weekday > 7 ? 6 : p_data->weekday - 1]);

    // 环境温湿度（温度、湿度均为整型，除以10得到一位小数）
    lv_label_set_text_fmt(ui_LabelTemp, "%.1f", (float)p_data->temperature / 10.0f);
    lv_label_set_text_fmt(ui_LabelHumi, "%.1f", (float)p_data->humidity / 10.0f);

    // 姿态角（pitch/roll/yaw 同样除以10）
    lv_label_set_text_fmt(ui_LabelPitch, "%.1f", (float)p_data->pitch / 10.0f);
    lv_label_set_text_fmt(ui_LabelRoll, "%.1f", (float)p_data->roll / 10.0f);
    lv_label_set_text_fmt(ui_LabelYaw, "%.1f", (float)p_data->yaw / 10.0f);

    /* ========== 天气屏幕 (ui_ScreenWeather) ========== */
    {
        // 天气描述和图标
        uint8_t code = p_data->weather_code;
        switch (code) {
        case WEATHER_CODE_SUNNY: // 请确保宏已定义
            lv_label_set_text(ui_LabelWeather, "Sunny");
            lv_img_set_src(ui_ImageWeather, &ui_img_sunny_png);
            break;
        case WEATHER_CODE_CLOUDY:
            lv_label_set_text(ui_LabelWeather, "Cloudy");
            lv_img_set_src(ui_ImageWeather, &ui_img_cloudy_png);
            break;
        case WEATHER_CODE_RAINY:
            lv_label_set_text(ui_LabelWeather, "Rainy");
            lv_img_set_src(ui_ImageWeather, &ui_img_rain_png);
            break;
        case WEATHER_CODE_OVERCAST:
            lv_label_set_text(ui_LabelWeather, "Overcast");
            lv_img_set_src(ui_ImageWeather, &ui_img_overcast_png);
            break;
        case WEATHER_CODE_SNOWY:
            lv_label_set_text(ui_LabelWeather, "Snowy");
            lv_img_set_src(ui_ImageWeather, &ui_img_snow_png);
            break;
        default:
            lv_label_set_text(ui_LabelWeather, "Unknown");
            break;
        }

        // 天气数据（温度、湿度、气压、风速）

        lv_label_set_text_fmt(ui_LabelWeatherTemp, "%d", p_data->weather_temp);
        lv_label_set_text_fmt(ui_LabelWeatherHumi, "%d%%", p_data->weather_humi);
        lv_label_set_text_fmt(ui_LabelWeatherPressure, "%d", p_data->weather_pressure);
        // 注意：weather_wind 若单位是 0.1m/s，则除以10；若已是整数 m/s，则直接使用
        lv_label_set_text_fmt(ui_LabelWeatherWind, "%d", p_data->weather_wind);

        // 城市（若结构体无城市字段，可留空或从外部获取）
        lv_label_set_text(ui_LabelWeatherCity, p_data->weather_city);

        // 天气更新时间
        lv_label_set_text_fmt(ui_LabelWeatherdate, "%04d-%02d-%02d",
                              p_data->weather_update_year,
                              p_data->weather_update_month,
                              p_data->weather_update_day);
    }

    /* ========== 设置屏幕 (ui_ScreenSet) ========== */
    // 阈值显示（由于只有一个滑块和一对高低标签，全部刷新时默认显示温度阈值）
    // 如果UI设计允许切换显示不同阈值，建议根据当前选中的类型单独更新，
    // 此处仅展示温度阈值作为示例，其他阈值事件请通过原事件函数单独触发。
    lv_label_set_text_fmt(ui_LabelMinValue, "%d", p_data->thresholds[threshold_type].threshold_low);
    lv_label_set_text_fmt(ui_LabelMaxValue, "%d", p_data->thresholds[threshold_type].threshold_high);
    lv_slider_set_value(ui_SliderThreshold, p_data->thresholds[threshold_type].threshold_high, LV_ANIM_OFF);
    lv_slider_set_left_value(ui_SliderThreshold, p_data->thresholds[threshold_type].threshold_low, LV_ANIM_OFF);
    // 滑块可以设置为低阈值或平均，这里设为低阈值
    lv_slider_set_range(ui_SliderThreshold, thresholds_range[threshold_type].threshold_low, thresholds_range[threshold_type].threshold_high);

    // 音量滑块
    lv_slider_set_value(ui_SliderVolume, p_data->volume, LV_ANIM_OFF);

    // 背景颜色（映射为LVGL颜色并应用到主屏幕，可扩展至其他屏幕）
    {
        lv_color_t color;
        switch (p_data->background_color_code) {
        case 0: color = lv_color_hex(0xFFFFFF); break; // 白色
        case 1: color = lv_color_hex(0xED9FD7); break; // 粉色
        case 2: color = lv_color_hex(0x9FC3ED); break; // 蓝色
        case 3: color = lv_color_hex(0xC196F5); break; // 紫色
        default:
            color = lv_color_hex(0xFFFFFF);
            break;
        }
        lv_dropdown_set_selected(ui_DropdownColor, p_data->background_color_code);

        lv_obj_set_style_bg_color(ui_ScreenMain, color, LV_PART_MAIN);
        // 如需同时修改其他屏幕背景，取消注释以下行
        lv_obj_set_style_bg_color(ui_ScreenWeather, color, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ui_ScreenMusic, color, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ui_ScreenSet, color, LV_PART_MAIN);
    }
}