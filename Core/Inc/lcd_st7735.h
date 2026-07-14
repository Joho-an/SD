#ifndef __LCD_ST7735_H__
#define __LCD_ST7735_H__

#include "main.h"
#include "spi.h"

/* LCD 尺寸 */
#define LCD_WIDTH   128
#define LCD_HEIGHT  128

/* 行列偏移（根据屏幕调整）*/
#define LCD_COL_OFFSET   2
#define LCD_ROW_OFFSET   3

/* 颜色模式：1=BGR, 0=RGB */
#define LCD_BGR_MODE     1

/* RGB565 常用颜色 */
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_CYAN      0x07FF
#define COLOR_YELLOW    0xFFE0
#define COLOR_MAGENTA   0xF81F

/* 引脚宏 */
#define LCD_CS_LOW()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET)
#define LCD_CS_HIGH()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET)
#define LCD_DC_LOW()    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET)
#define LCD_DC_HIGH()   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_SET)
#define LCD_BLK_ON()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET)
#define LCD_BLK_OFF()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET)

/* 基础函数 */
void LCD_Init(void);
void LCD_WriteCmd(uint8_t cmd);
void LCD_WriteData(uint8_t data);

/* 绘图函数 */
void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void LCD_Fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_FillScreen(uint16_t color);
void LCD_SetAddrWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye);

/* 文字函数 */
void LCD_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg);
void LCD_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg);

/* 批量写像素 */
void LCD_WritePixels(uint16_t *pixels, uint16_t count);

#endif /* __LCD_ST7735_H__ */
