#ifndef __LCD_CHINESE_H__
#define __LCD_CHINESE_H__

#include <stdint.h>
#include "ff.h"

/* HZK16 字库路径（SD卡根目录）*/
#define HZK16_PATH  "HZK16"

/* 在(x,y)处绘制一个16×16汉字，color=前景色，bg=背景色 */
/* bitmap指向32字节HZK16点阵数据 */
void LCD_DrawChinese(uint16_t x, uint16_t y, const uint8_t *bitmap, uint16_t color, uint16_t bg);

/* 从hzk文件取指定GB2312字符的点阵数据到bitmap[32] */
/* hi=区码(0xA1~0xFE)，lo=位码(0xA1~0xFE) */
/* 返回0=成功，非0=失败 */
int LCD_GetChineseBitmap(FIL *hzk, uint8_t hi, uint8_t lo, uint8_t *bitmap);

/* 打开HZK16字库文件，返回0=成功 */
int LCD_OpenHZK(FIL *hzk);

/* 关闭HZK16文件 */
void LCD_CloseHZK(FIL *hzk);

#endif
