#include "lcd_chinese.h"
#include "lcd_st7735.h"
#include "fatfs.h"

/* ========== 打开/关闭 HZK16 字库文件 ========== */

int LCD_OpenHZK(FIL *hzk)
{
  if (f_open(hzk, HZK16_PATH, FA_READ) != FR_OK)
    return -1;
  return 0;
}

void LCD_CloseHZK(FIL *hzk)
{
  f_close(hzk);
}

/* ========== 获取 GB2312 字符点阵 ========== */
/*
 * HZK16 编码方式：GB2312
 * 区码 = byte0 - 0xA1, 位码 = byte1 - 0xA1
 * 偏移 = (区码 × 94 + 位码) × 32
 * 每字 32 字节（16 行 × 2 字节/行，高位在前）
 */
int LCD_GetChineseBitmap(FIL *hzk, uint8_t hi, uint8_t lo, uint8_t *bitmap)
{
  if (hi < 0xA1 || hi > 0xFE || lo < 0xA1 || lo > 0xFE)
    return -1;  /* 超出 GB2312 范围 */

  uint8_t qu   = hi - 0xA1;
  uint8_t wei  = lo - 0xA1;
  uint32_t offset = ((uint32_t)qu * 94 + wei) * 32;

  UINT br;
  f_lseek(hzk, offset);
  if (f_read(hzk, bitmap, 32, &br) != FR_OK || br != 32)
    return -1;

  return 0;
}

/* ========== 绘制 16×16 汉字 ========== */
/*
 * HZK16 字节排列：
 *   第 0~1 字节：第 0 行左半（高位bit7）→右半（低位bit0）
 *   第 2~3 字节：第 1 行
 *   以此类推...
 *   先左后右：左 byte 的 bit7 = 该行最左像素
 */
void LCD_DrawChinese(uint16_t x, uint16_t y, const uint8_t *bitmap,
                     uint16_t color, uint16_t bg)
{
  for (uint8_t row = 0; row < 16; row++)
  {
    /* 每行 16 bit：bitmap[row*2] 左半，bitmap[row*2+1] 右半 */
    uint16_t line_data = ((uint16_t)bitmap[row * 2] << 8) | bitmap[row * 2 + 1];

    for (uint8_t col = 0; col < 16; col++)
    {
      if (line_data & (0x8000 >> col))
        LCD_DrawPixel(x + col, y + row, color);
      else
        if (bg != COLOR_BLACK)  /* 省去刷黑背景的耗时 */
          LCD_DrawPixel(x + col, y + row, bg);
    }
  }
}
