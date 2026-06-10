#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include "lvgl.h"

static void *sd_fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode);
static lv_fs_res_t sd_fs_close(lv_fs_drv_t *drv, void *file_p);
static lv_fs_res_t sd_fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br);
static lv_fs_res_t sd_fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence);
static lv_fs_res_t sd_fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p);

void lvgl_sd_fs_init()
{
    static bool initialized = false;
    if (initialized)
        return;

    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    fs_drv.letter = 'S';
    fs_drv.open_cb = sd_fs_open;
    fs_drv.close_cb = sd_fs_close;
    fs_drv.read_cb = sd_fs_read;
    fs_drv.seek_cb = sd_fs_seek;
    fs_drv.tell_cb = sd_fs_tell;
    lv_fs_drv_register(&fs_drv);

    initialized = true;
}

static String makeSdPath(const char *path)
{
    if (path != nullptr && path[0] == '/')
        return String(path);
    else if (path != nullptr)
        return "/" + String(path);
    return "/";
}

static void *sd_fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    LV_UNUSED(drv);

    const char *openMode = (mode == LV_FS_MODE_WR) ? FILE_WRITE : FILE_READ;
    File *file = new File(SD_MMC.open(makeSdPath(path).c_str(), openMode));
    if (!file || !(*file))
    {
        delete file;
        return nullptr;
    }

    return file;
}

static lv_fs_res_t sd_fs_close(lv_fs_drv_t *drv, void *file_p)
{
    LV_UNUSED(drv);

    File *file = static_cast<File *>(file_p);
    if (!file)
        return LV_FS_RES_INV_PARAM;

    file->close();
    delete file;
    return LV_FS_RES_OK;
}

static lv_fs_res_t sd_fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    LV_UNUSED(drv);

    File *file = static_cast<File *>(file_p);
    if (!file)
        return LV_FS_RES_INV_PARAM;

    size_t bytesRead = file->read(static_cast<uint8_t *>(buf), btr);
    if (br)
        *br = static_cast<uint32_t>(bytesRead);

    return LV_FS_RES_OK;
}

static lv_fs_res_t sd_fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    LV_UNUSED(drv);

    File *file = static_cast<File *>(file_p);
    if (!file)
        return LV_FS_RES_INV_PARAM;

    SeekMode seekMode = SeekSet;
    if (whence == LV_FS_SEEK_CUR)
        seekMode = SeekCur;
    else if (whence == LV_FS_SEEK_END)
        seekMode = SeekEnd;

    return file->seek(pos, seekMode) ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t sd_fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    LV_UNUSED(drv);

    File *file = static_cast<File *>(file_p);
    if (!file || !pos_p)
        return LV_FS_RES_INV_PARAM;

    *pos_p = static_cast<uint32_t>(file->position());
    return LV_FS_RES_OK;
}
