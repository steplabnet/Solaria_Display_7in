#ifndef _UI_IMG_MANAGER_H
#define _UI_IMG_MANAGER_H

#include "lvgl.h"

void _ui_load_binary(const char* fname, lv_img_dsc_t* dsc);

#define UI_LOAD_IMAGE _ui_load_binary

#endif
