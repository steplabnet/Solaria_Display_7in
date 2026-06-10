/*******************************************************************************
 * Size: 16 px
 * Bpp: 1
 * Opts: --bpp 1 --size 16 --font C:/Users/info/SquareLine/assets/Roboto_Condensed-Black.ttf -o C:/Users/info/SquareLine/assets\ui_font_Font1.c --format lvgl -r 0x20-0x7f --no-compress --no-prefilter
 ******************************************************************************/

#include "ui.h"

#ifndef UI_FONT_FONT1
#define UI_FONT_FONT1 1
#endif

#if UI_FONT_FONT1

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xff, 0xff, 0x1f, 0xf0,

    /* U+0022 "\"" */
    0xde, 0xf7, 0xb0,

    /* U+0023 "#" */
    0x34, 0x68, 0xf7, 0xff, 0xed, 0x9a, 0x7f, 0xfe,
    0xf1, 0x62, 0xc0,

    /* U+0024 "$" */
    0x8, 0x8, 0x1c, 0x3e, 0x77, 0x77, 0x70, 0x7c,
    0x3e, 0xf, 0x7, 0x77, 0x7f, 0x3e, 0x8, 0x8,

    /* U+0025 "%" */
    0x70, 0x7c, 0x36, 0x9b, 0xcf, 0xc3, 0xe0, 0x2c,
    0x3f, 0x15, 0x92, 0xc1, 0xe0, 0x60,

    /* U+0026 "&" */
    0x38, 0x3e, 0x1b, 0xd, 0x87, 0xc1, 0xc1, 0xed,
    0xfe, 0xef, 0x77, 0x3f, 0xcf, 0x60,

    /* U+0027 "'" */
    0xff,

    /* U+0028 "(" */
    0x33, 0x66, 0xcc, 0xcc, 0xcc, 0xcc, 0x46, 0x63,
    0x0,

    /* U+0029 ")" */
    0x8c, 0x66, 0x33, 0x33, 0x33, 0x33, 0x26, 0x6c,
    0x0,

    /* U+002A "*" */
    0x18, 0x23, 0x5b, 0xf3, 0x85, 0x99, 0x0,

    /* U+002B "+" */
    0x38, 0x70, 0xe1, 0xcf, 0xff, 0xce, 0x1c, 0x38,

    /* U+002C "," */
    0x6d, 0xbc,

    /* U+002D "-" */
    0xff,

    /* U+002E "." */
    0xff, 0x80,

    /* U+002F "/" */
    0x18, 0x8c, 0x63, 0x11, 0x8c, 0x63, 0x31, 0x8c,
    0x0,

    /* U+0030 "0" */
    0x38, 0xfb, 0xbf, 0x7e, 0xfd, 0xfb, 0xf7, 0xef,
    0xdd, 0xf1, 0xc0,

    /* U+0031 "1" */
    0x1b, 0xff, 0x73, 0x9c, 0xe7, 0x39, 0xce, 0x70,

    /* U+0032 "2" */
    0x3c, 0xff, 0xbe, 0x70, 0xe3, 0x87, 0x1c, 0x70,
    0xe3, 0xff, 0xf0,

    /* U+0033 "3" */
    0x3c, 0x7f, 0x67, 0x7, 0x1f, 0x1e, 0x1f, 0x7,
    0x7, 0x67, 0x7e, 0x3c,

    /* U+0034 "4" */
    0xe, 0x1e, 0x1e, 0x3e, 0x3e, 0x6e, 0x6e, 0xff,
    0xff, 0xe, 0xe, 0xe,

    /* U+0035 "5" */
    0x7f, 0x7f, 0x60, 0x60, 0x7e, 0x7e, 0x27, 0x7,
    0x7, 0x67, 0x7e, 0x3c,

    /* U+0036 "6" */
    0x18, 0x71, 0xc7, 0xf, 0xdf, 0xfb, 0xf7, 0xef,
    0xdd, 0xf1, 0xc0,

    /* U+0037 "7" */
    0xff, 0xfc, 0x38, 0x61, 0xc3, 0x86, 0x1c, 0x38,
    0x61, 0xc3, 0x80,

    /* U+0038 "8" */
    0x7d, 0xff, 0xbf, 0x7f, 0xef, 0x9f, 0x77, 0xef,
    0xdd, 0xf9, 0xc0,

    /* U+0039 "9" */
    0x38, 0xfb, 0xbf, 0x7e, 0xfd, 0xff, 0xbf, 0xe,
    0x38, 0xe1, 0x80,

    /* U+003A ":" */
    0xff, 0x80, 0x3f, 0xe0,

    /* U+003B ";" */
    0xff, 0x80, 0x3, 0x6d, 0xe4,

    /* U+003C "<" */
    0x4, 0x77, 0xfc, 0xf1, 0xf1, 0xc1,

    /* U+003D "=" */
    0xff, 0xf0, 0x0, 0xff, 0xf0,

    /* U+003E ">" */
    0x83, 0x8f, 0x8f, 0x3f, 0xee, 0x20,

    /* U+003F "?" */
    0x7d, 0xff, 0xb8, 0x71, 0xe3, 0x8c, 0x18, 0x0,
    0x70, 0xe1, 0xc0,

    /* U+0040 "@" */
    0xf, 0x7, 0xf1, 0xc7, 0x37, 0x6e, 0xf7, 0xb6,
    0xf6, 0xde, 0xdb, 0xda, 0x7b, 0x5f, 0x7f, 0x7e,
    0xc7, 0x0, 0x7c, 0x7, 0x80,

    /* U+0041 "A" */
    0xc, 0x7, 0x81, 0xe0, 0x78, 0x3f, 0xf, 0xc3,
    0x31, 0xcc, 0x7f, 0x9f, 0xe6, 0x1b, 0x87,

    /* U+0042 "B" */
    0xfc, 0xff, 0xe7, 0xe7, 0xe7, 0xfe, 0xff, 0xe7,
    0xe7, 0xe7, 0xfe, 0xfc,

    /* U+0043 "C" */
    0x3c, 0x7e, 0xe7, 0xe7, 0xe0, 0xe0, 0xe0, 0xe0,
    0xe7, 0xe7, 0x7e, 0x3c,

    /* U+0044 "D" */
    0xfc, 0xfe, 0xee, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7,
    0xe7, 0xee, 0xfe, 0xfc,

    /* U+0045 "E" */
    0xff, 0xff, 0x87, 0xe, 0x1f, 0xbf, 0x70, 0xe1,
    0xc3, 0xff, 0xf0,

    /* U+0046 "F" */
    0xff, 0xff, 0x87, 0xe, 0x1f, 0xbf, 0x70, 0xe1,
    0xc3, 0x87, 0x0,

    /* U+0047 "G" */
    0x3c, 0x7e, 0xe7, 0xe7, 0xe0, 0xef, 0xef, 0xe7,
    0xe7, 0xe7, 0x7f, 0x3e,

    /* U+0048 "H" */
    0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xff, 0xff, 0xe7,
    0xe7, 0xe7, 0xe7, 0xe7,

    /* U+0049 "I" */
    0xff, 0xff, 0xff, 0xff, 0xf0,

    /* U+004A "J" */
    0xe, 0x1c, 0x38, 0x70, 0xe1, 0xc3, 0x87, 0xef,
    0xdf, 0xf1, 0xc0,

    /* U+004B "K" */
    0xe7, 0x73, 0xbb, 0x9d, 0xcf, 0xc7, 0xe3, 0xf1,
    0xfc, 0xee, 0x77, 0x39, 0xdc, 0xe0,

    /* U+004C "L" */
    0xe1, 0xc3, 0x87, 0xe, 0x1c, 0x38, 0x70, 0xe1,
    0xc3, 0xff, 0xf0,

    /* U+004D "M" */
    0xf1, 0xfe, 0x3f, 0xc7, 0xfd, 0xff, 0xbf, 0xf7,
    0xfe, 0xff, 0x57, 0xee, 0xfd, 0xdf, 0xbb, 0xf2,
    0x70,

    /* U+004E "N" */
    0xe7, 0xe7, 0xf7, 0xf7, 0xf7, 0xf7, 0xef, 0xef,
    0xef, 0xef, 0xe7, 0xe7,

    /* U+004F "O" */
    0x3e, 0x3f, 0xb9, 0xdc, 0x7e, 0x3f, 0x1f, 0x8f,
    0xc7, 0xe3, 0xf3, 0xdf, 0xc7, 0xc0,

    /* U+0050 "P" */
    0xfc, 0xfe, 0xe7, 0xe7, 0xe7, 0xe7, 0xfe, 0xfc,
    0xe0, 0xe0, 0xe0, 0xe0,

    /* U+0051 "Q" */
    0x3e, 0x3f, 0xb9, 0xdc, 0x7e, 0x3f, 0x1f, 0x8f,
    0xc7, 0xe3, 0xf3, 0xdf, 0xc7, 0xe0, 0x78, 0x8,

    /* U+0052 "R" */
    0xfc, 0xfe, 0xe7, 0xe7, 0xe7, 0xe7, 0xfe, 0xfc,
    0xee, 0xee, 0xe7, 0xe7,

    /* U+0053 "S" */
    0x1e, 0x1f, 0x9c, 0xee, 0x77, 0x81, 0xf0, 0x7c,
    0xf, 0x73, 0xb9, 0xdf, 0xc3, 0xc0,

    /* U+0054 "T" */
    0xff, 0xff, 0xc7, 0x3, 0x81, 0xc0, 0xe0, 0x70,
    0x38, 0x1c, 0xe, 0x7, 0x3, 0x80,

    /* U+0055 "U" */
    0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7,
    0xe7, 0xe7, 0x7e, 0x3c,

    /* U+0056 "V" */
    0xe3, 0xf1, 0xdc, 0xee, 0x67, 0x71, 0xb8, 0xdc,
    0x7c, 0x3e, 0xf, 0x7, 0x83, 0x80,

    /* U+0057 "W" */
    0xe6, 0x7e, 0x66, 0x6e, 0x66, 0xfe, 0x7f, 0xe7,
    0xfe, 0x7f, 0xe7, 0xfc, 0x3f, 0xc3, 0xbc, 0x39,
    0xc3, 0x9c,

    /* U+0058 "X" */
    0xe3, 0xbb, 0x9d, 0xc6, 0xc3, 0xe0, 0xe0, 0xf8,
    0x7c, 0x36, 0x3b, 0x9d, 0xdc, 0x70,

    /* U+0059 "Y" */
    0xe3, 0xb1, 0x9d, 0xce, 0xe3, 0x61, 0xf0, 0x70,
    0x38, 0x1c, 0xe, 0x7, 0x3, 0x80,

    /* U+005A "Z" */
    0xff, 0xff, 0x6, 0xe, 0x1c, 0x1c, 0x38, 0x38,
    0x70, 0x70, 0xff, 0xff,

    /* U+005B "[" */
    0xff, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xef,
    0xf0,

    /* U+005C "\\" */
    0xe1, 0xc1, 0x83, 0x87, 0x6, 0xe, 0x1c, 0x18,
    0x38, 0x70, 0xe0, 0xc0,

    /* U+005D "]" */
    0xff, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x7f,
    0xf0,

    /* U+005E "^" */
    0x30, 0xe7, 0x9a, 0x6f, 0x30,

    /* U+005F "_" */
    0xff, 0xf0,

    /* U+0060 "`" */
    0x71, 0x80,

    /* U+0061 "a" */
    0x7b, 0xff, 0xdf, 0xff, 0xff, 0xff, 0x7c,

    /* U+0062 "b" */
    0xe1, 0xc3, 0x87, 0xf, 0xdf, 0xfb, 0xf7, 0xef,
    0xdf, 0xbf, 0xff, 0xc0,

    /* U+0063 "c" */
    0x78, 0xfb, 0xb7, 0xe, 0x1c, 0x3b, 0x3e, 0x78,

    /* U+0064 "d" */
    0xe, 0x1c, 0x38, 0x77, 0xff, 0xfb, 0xf7, 0xef,
    0xdf, 0xbf, 0xf7, 0xe0,

    /* U+0065 "e" */
    0x3c, 0xfb, 0xbf, 0xff, 0xfc, 0x39, 0x3f, 0x3c,

    /* U+0066 "f" */
    0x3b, 0xdc, 0xef, 0xfd, 0xce, 0x73, 0x9c, 0xe7,
    0x0,

    /* U+0067 "g" */
    0x7f, 0xff, 0xbf, 0x7e, 0xfd, 0xfb, 0xff, 0x7e,
    0x1d, 0xfb, 0xc0,

    /* U+0068 "h" */
    0xe1, 0xc3, 0x87, 0xf, 0xdf, 0xfb, 0xf7, 0xef,
    0xdf, 0xbf, 0x7e, 0xe0,

    /* U+0069 "i" */
    0xfc, 0xf, 0xff, 0xff, 0xfe,

    /* U+006A "j" */
    0x39, 0xc0, 0x3, 0x9c, 0xe7, 0x39, 0xce, 0x73,
    0x9f, 0xfe,

    /* U+006B "k" */
    0xe0, 0xe0, 0xe0, 0xe0, 0xee, 0xee, 0xfc, 0xf8,
    0xfc, 0xfc, 0xec, 0xee, 0xe6,

    /* U+006C "l" */
    0xff, 0xff, 0xff, 0xff, 0xfe,

    /* U+006D "m" */
    0xfd, 0xdf, 0xff, 0xbb, 0xf7, 0x7e, 0xef, 0xdd,
    0xfb, 0xbf, 0x77, 0xee, 0xe0,

    /* U+006E "n" */
    0xfd, 0xff, 0xbf, 0x7e, 0xfd, 0xfb, 0xf7, 0xee,

    /* U+006F "o" */
    0x38, 0xfb, 0xbf, 0x7e, 0xfd, 0xfb, 0xbe, 0x38,

    /* U+0070 "p" */
    0xfd, 0xff, 0xbf, 0x7e, 0xfd, 0xfb, 0xff, 0xfd,
    0xc3, 0x87, 0x0,

    /* U+0071 "q" */
    0x7f, 0xff, 0xbf, 0x7e, 0xfd, 0xfb, 0xff, 0x7e,
    0x1c, 0x38, 0x70,

    /* U+0072 "r" */
    0xff, 0xf9, 0xce, 0x73, 0x9c, 0xe0,

    /* U+0073 "s" */
    0x79, 0xfb, 0xb7, 0x7, 0x83, 0xb3, 0xfe, 0x78,

    /* U+0074 "t" */
    0x73, 0xbf, 0xf7, 0x39, 0xce, 0x73, 0xce,

    /* U+0075 "u" */
    0xef, 0xdf, 0xbf, 0x7e, 0xfd, 0xfb, 0xff, 0x7e,

    /* U+0076 "v" */
    0xef, 0xdf, 0xbb, 0x66, 0xcf, 0x9f, 0x1c, 0x38,

    /* U+0077 "w" */
    0x64, 0xcd, 0xd9, 0xbb, 0x3f, 0xe7, 0xbc, 0xf7,
    0x8e, 0xe1, 0xdc, 0x3b, 0x80,

    /* U+0078 "x" */
    0xee, 0xdd, 0xf1, 0xe3, 0x87, 0x9f, 0x37, 0xee,

    /* U+0079 "y" */
    0xef, 0xdd, 0xbb, 0x67, 0xcf, 0x8e, 0x1c, 0x38,
    0x71, 0xc3, 0x0,

    /* U+007A "z" */
    0xff, 0xf1, 0x8e, 0x31, 0x86, 0x3f, 0xfc,

    /* U+007B "{" */
    0x3, 0x66, 0x66, 0x6e, 0xce, 0x66, 0x66, 0x63,
    0x0,

    /* U+007C "|" */
    0xff, 0xfc,

    /* U+007D "}" */
    0xc, 0x66, 0x66, 0x67, 0x37, 0x66, 0x66, 0x6c,
    0x0,

    /* U+007E "~" */
    0x73, 0xf6, 0xde, 0xcc
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 59, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 68, .box_w = 3, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 6, .adv_w = 81, .box_w = 5, .box_h = 4, .ofs_x = 1, .ofs_y = 9},
    {.bitmap_index = 9, .adv_w = 131, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 20, .adv_w = 131, .box_w = 8, .box_h = 16, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 36, .adv_w = 165, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 50, .adv_w = 153, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 64, .adv_w = 40, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = 9},
    {.bitmap_index = 65, .adv_w = 83, .box_w = 4, .box_h = 17, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 74, .adv_w = 83, .box_w = 4, .box_h = 17, .ofs_x = 0, .ofs_y = -4},
    {.bitmap_index = 83, .adv_w = 119, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 90, .adv_w = 119, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 98, .adv_w = 69, .box_w = 3, .box_h = 5, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 100, .adv_w = 108, .box_w = 4, .box_h = 2, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 101, .adv_w = 78, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 103, .adv_w = 79, .box_w = 5, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 112, .adv_w = 131, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 123, .adv_w = 131, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 131, .adv_w = 131, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 142, .adv_w = 131, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 154, .adv_w = 131, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 166, .adv_w = 131, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 178, .adv_w = 131, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 189, .adv_w = 131, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 200, .adv_w = 131, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 211, .adv_w = 131, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 222, .adv_w = 75, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 226, .adv_w = 72, .box_w = 3, .box_h = 13, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 231, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 237, .adv_w = 132, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 242, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 248, .adv_w = 118, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 259, .adv_w = 196, .box_w = 11, .box_h = 15, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 280, .adv_w = 155, .box_w = 10, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 295, .adv_w = 146, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 307, .adv_w = 147, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 319, .adv_w = 144, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 331, .adv_w = 125, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 342, .adv_w = 122, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 353, .adv_w = 151, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 365, .adv_w = 156, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 377, .adv_w = 72, .box_w = 3, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 382, .adv_w = 126, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 393, .adv_w = 143, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 407, .adv_w = 123, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 418, .adv_w = 194, .box_w = 11, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 435, .adv_w = 156, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 447, .adv_w = 155, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 461, .adv_w = 147, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 473, .adv_w = 155, .box_w = 9, .box_h = 14, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 489, .adv_w = 145, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 501, .adv_w = 141, .box_w = 9, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 515, .adv_w = 142, .box_w = 9, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 529, .adv_w = 148, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 541, .adv_w = 150, .box_w = 9, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 555, .adv_w = 190, .box_w = 12, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 573, .adv_w = 144, .box_w = 9, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 587, .adv_w = 141, .box_w = 9, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 601, .adv_w = 137, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 613, .adv_w = 69, .box_w = 4, .box_h = 17, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 622, .adv_w = 99, .box_w = 7, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 634, .adv_w = 69, .box_w = 4, .box_h = 17, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 643, .adv_w = 103, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 648, .adv_w = 101, .box_w = 6, .box_h = 2, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 650, .adv_w = 87, .box_w = 5, .box_h = 2, .ofs_x = 0, .ofs_y = 10},
    {.bitmap_index = 652, .adv_w = 119, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 659, .adv_w = 127, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 671, .adv_w = 117, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 679, .adv_w = 127, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 691, .adv_w = 124, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 699, .adv_w = 85, .box_w = 5, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 708, .adv_w = 130, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 719, .adv_w = 127, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 731, .adv_w = 67, .box_w = 3, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 736, .adv_w = 65, .box_w = 5, .box_h = 16, .ofs_x = -1, .ofs_y = -3},
    {.bitmap_index = 746, .adv_w = 126, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 759, .adv_w = 67, .box_w = 3, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 764, .adv_w = 189, .box_w = 11, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 777, .adv_w = 127, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 785, .adv_w = 126, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 793, .adv_w = 127, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 804, .adv_w = 127, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 815, .adv_w = 88, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 821, .adv_w = 116, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 829, .adv_w = 80, .box_w = 5, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 836, .adv_w = 127, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 844, .adv_w = 118, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 852, .adv_w = 160, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 865, .adv_w = 117, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 873, .adv_w = 118, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 884, .adv_w = 117, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 891, .adv_w = 75, .box_w = 4, .box_h = 17, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 900, .adv_w = 66, .box_w = 1, .box_h = 14, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 902, .adv_w = 75, .box_w = 4, .box_h = 17, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 911, .adv_w = 139, .box_w = 8, .box_h = 4, .ofs_x = 1, .ofs_y = 3}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Pair left and right glyphs for kerning*/
static const uint8_t kern_pair_glyph_ids[] =
{
    3, 3,
    3, 8,
    8, 3,
    8, 8,
    13, 3,
    13, 8,
    15, 3,
    15, 8,
    16, 16
};

/* Kerning between the respective left and right glyphs
 * 4.4 format which needs to scaled with `kern_scale`*/
static const int8_t kern_pair_values[] =
{
    -5, -5, -5, -5, -36, -36, -36, -36,
    -31
};

/*Collect the kern pair's data in one place*/
static const lv_font_fmt_txt_kern_pair_t kern_pairs =
{
    .glyph_ids = kern_pair_glyph_ids,
    .values = kern_pair_values,
    .pair_cnt = 9,
    .glyph_ids_size = 0
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_pairs,
    .kern_scale = 16,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t ui_font_Font1 = {
#else
lv_font_t ui_font_Font1 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 18,          /*The maximum line height required by the font*/
    .base_line = 4,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if UI_FONT_FONT1*/

