/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lcd_st7735.c
  * @brief   ST7735 1.44" 128x128 TFT LCD Driver (SPI)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "lcd_st7735.h"

/* 5x7 ASCII 字库 (0x20~0x7E) */
/* 每个字符5字节，每字节代表一列，bit0~bit6代表行 */
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* SPACE */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
    {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */
    {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* * */
    {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x00,0x08,0x14,0x22,0x41}, /* < */
    {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x41,0x22,0x14,0x08,0x00}, /* > */
    {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x01,0x01}, /* F */
    {0x3E,0x41,0x41,0x51,0x32}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x04,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x7F,0x20,0x18,0x20,0x7F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x03,0x04,0x78,0x04,0x03}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x00,0x00,0x7F,0x41,0x41}, /* [ */
    {0x02,0x04,0x08,0x10,0x20}, /* \ */
    {0x41,0x41,0x7F,0x00,0x00}, /* ] */
    {0x04,0x02,0x01,0x02,0x04}, /* ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* _ */
    {0x00,0x01,0x02,0x04,0x00}, /* ` */
    {0x20,0x54,0x54,0x54,0x78}, /* a */
    {0x7F,0x48,0x44,0x44,0x38}, /* b */
    {0x38,0x44,0x44,0x44,0x20}, /* c */
    {0x38,0x44,0x44,0x48,0x7F}, /* d */
    {0x38,0x54,0x54,0x54,0x18}, /* e */
    {0x08,0x7E,0x09,0x01,0x02}, /* f */
    {0x08,0x14,0x54,0x54,0x3C}, /* g */
    {0x7F,0x08,0x04,0x04,0x78}, /* h */
    {0x00,0x44,0x7D,0x40,0x00}, /* i */
    {0x20,0x40,0x44,0x3D,0x00}, /* j */
    {0x00,0x7F,0x10,0x28,0x44}, /* k */
    {0x00,0x41,0x7F,0x40,0x00}, /* l */
    {0x7C,0x04,0x18,0x04,0x78}, /* m */
    {0x7C,0x08,0x04,0x04,0x78}, /* n */
    {0x38,0x44,0x44,0x44,0x38}, /* o */
    {0x7C,0x14,0x14,0x14,0x08}, /* p */
    {0x08,0x14,0x14,0x18,0x7C}, /* q */
    {0x7C,0x08,0x04,0x04,0x08}, /* r */
    {0x48,0x54,0x54,0x54,0x20}, /* s */
    {0x04,0x3F,0x44,0x40,0x20}, /* t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* w */
    {0x44,0x28,0x10,0x28,0x44}, /* x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* y */
    {0x44,0x64,0x54,0x4C,0x44}, /* z */
    {0x00,0x08,0x36,0x41,0x00}, /* { */
    {0x00,0x00,0x7F,0x00,0x00}, /* | */
    {0x00,0x41,0x36,0x08,0x00}, /* } */
    {0x08,0x08,0x2A,0x1C,0x08}, /* ~ */
};

/* 私有函数声明 */
static void LCD_Reset(void);
static void LCD_WriteData16(uint16_t data);

/* ========== 底层SPI写 ========== */

void LCD_WriteCmd(uint8_t cmd)
{
  LCD_CS_LOW();
  LCD_DC_LOW();   /* 命令模式 */
  HAL_SPI_Transmit(&hspi2, &cmd, 1, 100);
  LCD_CS_HIGH();
}

void LCD_WriteData(uint8_t data)
{
  LCD_CS_LOW();
  LCD_DC_HIGH();  /* 数据模式 */
  HAL_SPI_Transmit(&hspi2, &data, 1, 100);
  LCD_CS_HIGH();
}

/* 将 RGB565 颜色拆成 2 字节（按字节序配置）*/
static void LCD_ColorToBytes(uint16_t color, uint8_t *hi, uint8_t *lo)
{
  *hi = (color >> 8) & 0xFF;
  *lo = color & 0xFF;
}

static void LCD_WriteData16(uint16_t data)
{
  uint8_t buf[2];
  LCD_ColorToBytes(data, &buf[0], &buf[1]);
  LCD_CS_LOW();
  LCD_DC_HIGH();
  HAL_SPI_Transmit(&hspi2, buf, 2, 100);
  LCD_CS_HIGH();
}

/* ========== 硬件复位 ========== */

static void LCD_Reset(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* 将 PB14 (SPI2_MISO) 重配为 GPIO 输出，用作 LCD_RST */
  GPIO_InitStruct.Pin = GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* 复位时序 */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
  HAL_Delay(10);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
  HAL_Delay(150);
}

/* ========== ST7735 初始化序列 ========== */

void LCD_Init(void)
{
  /* 初始化 CS、DC、BLK 为输出（CubeMX 已配置，但确认一下） */
  LCD_BLK_OFF();
  LCD_CS_HIGH();

  /* 硬件复位 */
  LCD_Reset();

  /* --- 初始化命令序列（ST7735S / 128x128） --- */

  /* Sleep Out */
  LCD_WriteCmd(0x11);
  HAL_Delay(120);

  /* Frame Rate Control 1 (Normal) */
  LCD_WriteCmd(0xB1);
  LCD_WriteData(0x05);
  LCD_WriteData(0x3A);
  LCD_WriteData(0x3A);

  /* Frame Rate Control 2 (Idle) */
  LCD_WriteCmd(0xB2);
  LCD_WriteData(0x05);
  LCD_WriteData(0x3A);
  LCD_WriteData(0x3A);

  /* Frame Rate Control 3 (Partial) */
  LCD_WriteCmd(0xB3);
  LCD_WriteData(0x05);
  LCD_WriteData(0x3A);
  LCD_WriteData(0x3A);
  LCD_WriteData(0x05);
  LCD_WriteData(0x3A);
  LCD_WriteData(0x3A);

  /* Display Inversion Control */
  LCD_WriteCmd(0xB4);
  LCD_WriteData(0x03);

  /* Power Control 1 */
  LCD_WriteCmd(0xC0);
  LCD_WriteData(0x28);
  LCD_WriteData(0x08);
  LCD_WriteData(0x04);

  /* Power Control 2 */
  LCD_WriteCmd(0xC1);
  LCD_WriteData(0xC0);

  /* Power Control 3 (Normal) */
  LCD_WriteCmd(0xC2);
  LCD_WriteData(0x0D);
  LCD_WriteData(0x00);

  /* Power Control 4 (Idle) */
  LCD_WriteCmd(0xC3);
  LCD_WriteData(0x8D);
  LCD_WriteData(0x2A);

  /* Power Control 5 (Partial) */
  LCD_WriteCmd(0xC4);
  LCD_WriteData(0x8D);
  LCD_WriteData(0xEE);

  /* VCOM Control 1 */
  LCD_WriteCmd(0xC5);
  LCD_WriteData(0x1A);

  /* Memory Data Access Control */
  LCD_WriteCmd(0x36);
#if LCD_BGR_MODE
  LCD_WriteData(0xC8);  /* MX | MV | BGR */
#else
  LCD_WriteData(0xC0);  /* MX | MV | RGB */
#endif

  /* Interface Pixel Format: 16-bit RGB565 */
  LCD_WriteCmd(0x3A);
  LCD_WriteData(0x05);

  /* Column Address Set — 设为全物理范围（不偏移，由 SetAddrWindow 处理） */
  LCD_WriteCmd(0x2A);
  LCD_WriteData(0x00);
  LCD_WriteData(0x00);          /* 起始列 = 0 */
  LCD_WriteData(0x00);
  LCD_WriteData(0x7F);          /* 结束列 = 127 */

  /* Row Address Set */
  LCD_WriteCmd(0x2B);
  LCD_WriteData(0x00);
  LCD_WriteData(0x00);          /* 起始行 = 0 */
  LCD_WriteData(0x00);
  LCD_WriteData(0x7F);          /* 结束行 = 127 */

  /* Display Inversion Off (大部分 ST7735 不需要反相) */
  LCD_WriteCmd(0x20);

  /* Normal Display Mode On */
  LCD_WriteCmd(0x13);
  HAL_Delay(10);

  /* Display On */
  LCD_WriteCmd(0x29);
  HAL_Delay(100);

  /* 打开背光 */
  LCD_BLK_ON();
}

/* ========== 绘图函数 ========== */

void LCD_SetAddrWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye)
{
  uint16_t col_start = xs + LCD_COL_OFFSET;
  uint16_t col_end   = xe + LCD_COL_OFFSET;
  uint16_t row_start = ys + LCD_ROW_OFFSET;
  uint16_t row_end   = ye + LCD_ROW_OFFSET;

  LCD_WriteCmd(0x2A);
  LCD_WriteData(col_start >> 8);
  LCD_WriteData(col_start & 0xFF);
  LCD_WriteData(col_end >> 8);
  LCD_WriteData(col_end & 0xFF);

  LCD_WriteCmd(0x2B);
  LCD_WriteData(row_start >> 8);
  LCD_WriteData(row_start & 0xFF);
  LCD_WriteData(row_end >> 8);
  LCD_WriteData(row_end & 0xFF);

  LCD_WriteCmd(0x2C);  /* RAMWR */
}

void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
  if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
  LCD_SetAddrWindow(x, y, x, y);
  LCD_WriteData16(color);
}

void LCD_Fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  uint32_t total = (uint32_t)w * h;
  uint8_t hi, lo;
  uint8_t buf[256];  /* 批量发送缓冲区 */

  LCD_ColorToBytes(color, &hi, &lo);

  if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
  if (x + w > LCD_WIDTH)  w = LCD_WIDTH - x;
  if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

  LCD_SetAddrWindow(x, y, x + w - 1, y + h - 1);

  LCD_CS_LOW();
  LCD_DC_HIGH();

  /* 批量填充：在 buf 中构造 color 数据 */
  uint32_t buf_words = (total > 128) ? 128 : total;
  for (uint32_t i = 0; i < buf_words; i++)
  {
    buf[i * 2]     = hi;
    buf[i * 2 + 1] = lo;
  }

  uint32_t sent = 0;
  while (sent < total)
  {
    uint32_t chunk = (total - sent > buf_words) ? buf_words : (total - sent);
    HAL_SPI_Transmit(&hspi2, buf, chunk * 2, 1000);
    sent += chunk;
  }

  LCD_CS_HIGH();
}

void LCD_FillScreen(uint16_t color)
{
  LCD_Fill(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

/* ========== 文字函数 ========== */

void LCD_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg)
{
  uint16_t buf[6 * 8];  /* 一个字符的像素缓冲区 */
  uint8_t idx;

  if (c < 0x20 || c > 0x7E) c = ' ';
  idx = c - 0x20;

  for (uint8_t row = 0; row < 8; row++)
  {
    for (uint8_t col = 0; col < 6; col++)
    {
      if (col < 5)
      {
        if (font5x7[idx][col] & (1 << row))
          buf[row * 6 + col] = color;
        else
          buf[row * 6 + col] = bg;
      }
      else
      {
        buf[row * 6 + col] = bg;  /* 字间距 */
      }
    }
  }

  LCD_SetAddrWindow(x, y, x + 5, y + 7);
  LCD_CS_LOW();
  LCD_DC_HIGH();
  for (uint32_t i = 0; i < 48; i++)
  {
    uint8_t hi, lo;
    LCD_ColorToBytes(buf[i], &hi, &lo);
    HAL_SPI_Transmit(&hspi2, &hi, 1, 100);
    HAL_SPI_Transmit(&hspi2, &lo, 1, 100);
  }
  LCD_CS_HIGH();
}

void LCD_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg)
{
  while (*str)
  {
    LCD_DrawChar(x, y, *str, color, bg);
    x += 6;  /* 字符宽5 + 间距1 */
    if (x + 6 > LCD_WIDTH)
    {
      x = 0;
      y += 8;
    }
    str++;
  }
}

void LCD_WritePixels(uint16_t *pixels, uint16_t count)
{
  uint8_t buf[256];
  uint16_t i = 0;
  while (i < count)
  {
    uint16_t n = (count - i > 128) ? 128 : (count - i);
    for (uint16_t j = 0; j < n; j++)
    {
      LCD_ColorToBytes(pixels[i + j], &buf[j * 2], &buf[j * 2 + 1]);
    }
    HAL_SPI_Transmit(&hspi2, buf, n * 2, 5000);
    i += n;
  }
}

/* LCD_BLK_ON / LCD_BLK_OFF 宏直接在外部使用即可 */
