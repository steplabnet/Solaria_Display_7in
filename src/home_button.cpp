#include "home_buttons.h"

void UpdateTempLabelBackgrounds(TYPE_DISPLAY_DATA display)
{
    lv_obj_t *tempLabels[12] = {
        ui_tempLabel1, ui_tempLabel2, ui_tempLabel3, ui_tempLabel4,
        ui_tempLabel5, ui_tempLabel6, ui_tempLabel7, ui_tempLabel8,
        ui_tempLabel9, ui_tempLabel10, ui_tempLabel11, ui_tempLabel12
    };

    for (int i = 0; i < 12; i++)
    {
        if (display.mode[i] == 0 || display.mode[i] == 1)
        {
            lv_obj_set_style_bg_color(tempLabels[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(tempLabels[i], LV_OPA_50, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(tempLabels[i], 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(tempLabels[i], 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(tempLabels[i], 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(tempLabels[i], 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(tempLabels[i], 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        else
        {
            lv_obj_set_style_bg_opa(tempLabels[i], LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

void UpdateBtnLabels(TYPE_DISPLAY_DATA display, String *oldLabels)
{
    lv_obj_t *labels[12] = {
        ui_labelBtn1, ui_labelBtn2, ui_labelBtn3, ui_labelBtn4,
        ui_labelBtn5, ui_labelBtn6, ui_labelBtn7, ui_labelBtn8,
        ui_labelBtn9, ui_labelBtn10, ui_labelBtn11, ui_labeBtn12
    };

    for (int i = 0; i < 12; i++)
    {
        // Include mode in cache key so a mode change forces a refresh
        String cacheKey = display.btnLabel[i] + "|" + String(display.mode[i]);
        if (cacheKey != oldLabels[i])
        {
            oldLabels[i] = cacheKey;
            lv_label_set_text(labels[i], display.btnLabel[i].c_str());
        }
    }
};

void UpdateBtnLeds(TYPE_DISPLAY_DATA display)
{
    lv_obj_t *leds[12] = {
        ui_ledBtn1, ui_ledBtn2, ui_ledBtn3, ui_ledBtn4,
        ui_ledBtn5, ui_ledBtn6, ui_ledBtn7, ui_ledBtn8,
        ui_ledBtn9, ui_ledBtn10, ui_ledBtn11, ui_ledBtn12
    };

    for (int i = 0; i < 12; i++)
    {
        if (display.mode[i] == 2)
        {
            lv_obj_set_style_bg_color(leds[i],
                display.btnEn[i] ? lv_color_hex(0x00AA00) : lv_color_hex(0xCC0000),
                LV_PART_MAIN);
            lv_obj_clear_flag(leds[i], LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(leds[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void UpdateBtnTemperatures(TYPE_DISPLAY_DATA display)
{
    lv_obj_t *tempLabels[12] = {
        ui_tempLabel1, ui_tempLabel2, ui_tempLabel3, ui_tempLabel4,
        ui_tempLabel5, ui_tempLabel6, ui_tempLabel7, ui_tempLabel8,
        ui_tempLabel9, ui_tempLabel10, ui_tempLabel11, ui_tempLabel12
    };

    UpdateTempLabelBackgrounds(display);

    for (int i = 0; i < 12; i++)
    {
        if (display.temperature[i].length() > 0 && display.temperature[i] != "--")
        {
            String t = display.temperature[i] + "\xC2\xB0""C";
            lv_label_set_text(tempLabels[i], t.c_str());
            lv_obj_clear_flag(tempLabels[i], LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(tempLabels[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}
