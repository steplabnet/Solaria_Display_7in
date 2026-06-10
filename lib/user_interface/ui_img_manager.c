#include "ui.h"

void _ui_load_binary(const char* fname, lv_img_dsc_t* dsc)
{
    lv_fs_file_t f;
    if (lv_fs_open(&f, fname, LV_FS_MODE_RD) != LV_FS_RES_OK) {
        LV_LOG_WARN("_ui_load_binary: cannot open %s", fname);
        return;
    }

    // Read the 4-byte LVGL header embedded by the converter
    lv_img_header_t header;
    uint32_t read_num;
    lv_fs_read(&f, &header, sizeof(header), &read_num);

    // Compute pixel data size from header
    uint32_t px_size;
    switch (header.cf) {
        case LV_IMG_CF_TRUE_COLOR_ALPHA: px_size = 3; break;
        case LV_IMG_CF_TRUE_COLOR:       px_size = 2; break;
        default:                         px_size = 2; break;
    }
    uint32_t data_size = (uint32_t)header.w * header.h * px_size;

    uint8_t* buf = (uint8_t*)lv_mem_alloc(data_size);
    if (!buf) {
        LV_LOG_WARN("_ui_load_binary: alloc failed (%u bytes)", data_size);
        lv_fs_close(&f);
        return;
    }

    lv_fs_read(&f, buf, data_size, &read_num);
    lv_fs_close(&f);

    dsc->header    = header;
    dsc->data      = buf;
    dsc->data_size = data_size;
}
