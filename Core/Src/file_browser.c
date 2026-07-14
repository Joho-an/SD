#include "file_browser.h"
#include "audio_player.h"
#include "fatfs.h"
#include "lcd_st7735.h"
#include "main.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>

#define MAX_FILES    30
#define NAME_LEN     32
#define MAX_VISIBLE  13

static char file_list[MAX_FILES][NAME_LEN];
static uint8_t file_cnt;

/* ===================== 显示 BMP ===================== */
static void display_bmp(const char *name)
{
  FIL fil;
  UINT br;
  FRESULT res;

  extern SD_HandleTypeDef hsd;
  SDIO->CLKCR = (SDIO->CLKCR & ~SDIO_CLKCR_CLKDIV_Msk)
              | (hsd.Init.ClockDiv << SDIO_CLKCR_CLKDIV_Pos);

  res = f_open(&fil, name, FA_READ);
  if (res != FR_OK) return;

  uint8_t bmp_hdr[54];
  f_read(&fil, bmp_hdr, 54, &br);
  if (br == 54 && bmp_hdr[0]=='B' && bmp_hdr[1]=='M')
  {
    uint32_t data_off = bmp_hdr[10]|(bmp_hdr[11]<<8)|(bmp_hdr[12]<<16)|(bmp_hdr[13]<<24);
    int32_t w = bmp_hdr[18]|(bmp_hdr[19]<<8)|(bmp_hdr[20]<<16)|(bmp_hdr[21]<<24);
    int32_t h = bmp_hdr[22]|(bmp_hdr[23]<<8)|(bmp_hdr[24]<<16)|(bmp_hdr[25]<<24);
    uint16_t bpp = bmp_hdr[28]|(bmp_hdr[29]<<8);

    if ((bpp == 24 || bpp == 32) && w <= 128 && h <= 128)
    {
      uint32_t row_sz = ((w * bpp + 31) / 32) * 4;
      uint8_t row_buf[512];
	      uint8_t bpp_div = bpp / 8;                 /* 3=24-bit, 4=32-bit */


      for (int32_t row = 0; row < h; row++)
      {
        uint32_t file_row = h - 1 - row;
        f_lseek(&fil, data_off + file_row * row_sz);
        f_read(&fil, row_buf, w * bpp_div, &br);
        if (br != (UINT)(w * bpp_div)) break;

        uint16_t pixels[128];
        for (int32_t i = 0; i < w; i++)
          /* BMP: BGR byte order in file; LCD is RGB-wired with MADCTL BGR=1
             so send RGB565 — the BGR flag swaps it back to match the hardware */
          pixels[i] = ((row_buf[i*bpp_div+2]>>3)<<11) | ((row_buf[i*bpp_div+1]>>2)<<5) | (row_buf[i*bpp_div]>>3);

        LCD_SetAddrWindow(0, row, w-1, row);
        LCD_CS_LOW(); LCD_DC_HIGH();
        LCD_WritePixels(pixels, w);
        LCD_CS_HIGH();

        if (row % 16 == 0) osDelay(1);
      }
    }
  }
  f_close(&fil);
}

/* ===================== TXT 阅读进度 ===================== */
#define TXT_LINE_LEN        256   /* 每行最大字节数（中文占2字节/字）*/
#define TXT_LINES_PER_PAGE  14    /* 每页行数（8px行高 ×14 = 112px）*/
#define TXT_ROW_HEIGHT      8     /* ASCII行高 */
#define TXT_CHARS_PER_LINE  21    /* LCD 128px / 6px每字 = 21字 */
#define TXT_PROGRESS_FILE   "txt_prog.bin"
#define UTF8_BOM_HI         0xEF
#define UTF8_BOM_MI         0xBB
#define UTF8_BOM_LO         0xBF

/* 保存当前文件阅读进度 */
static void txt_save_progress(const char *name, uint32_t offset)
{
  FIL f;
  UINT bw;
  if (f_open(&f, TXT_PROGRESS_FILE, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) return;
  uint16_t len = strlen(name);
  f_write(&f, &len, sizeof(len), &bw);
  f_write(&f, name, len, &bw);
  f_write(&f, &offset, sizeof(offset), &bw);
  f_close(&f);
}

/* 恢复上次阅读进度，返回文件偏移（0=无记录） */
static uint32_t txt_load_progress(const char *name)
{
  FIL f;
  UINT br;
  uint32_t offset = 0;
  if (f_open(&f, TXT_PROGRESS_FILE, FA_READ) != FR_OK) return 0;

  uint16_t len;
  if (f_read(&f, &len, sizeof(len), &br) == FR_OK && br == sizeof(len) && len < 32)
  {
    char saved_name[32];
    if (f_read(&f, saved_name, len, &br) == FR_OK && br == len)
    {
      saved_name[len] = '\0';
      if (strcmp(saved_name, name) == 0)
        f_read(&f, &offset, sizeof(offset), &br);
    }
  }
  f_close(&f);
  return offset;
}

/* 检测并跳过 UTF-8 BOM，返回跳过后的偏移 */
static uint32_t txt_skip_utf8_bom(FIL *fil)
{
  uint8_t bom[3];
  UINT br;
  uint32_t pos = f_tell(fil);
  f_read(fil, bom, 3, &br);
  if (br == 3 && bom[0] == UTF8_BOM_HI && bom[1] == UTF8_BOM_MI && bom[2] == UTF8_BOM_LO)
    return pos + 3;   /* 跳过 BOM */
  f_lseek(fil, pos);  /* 没有 BOM，回退 */
  return pos;
}

/* ===================== TXT 分页阅读器 ===================== */
/* 短按 = 下一页，长按 = 返回文件列表，自动记忆进度 */
static void display_txt_paged(const char *name)
{
  /* 恢复 SDIO 时钟（避免 BMP 播放后时钟改变导致读写失败）*/
  {
    extern SD_HandleTypeDef hsd;
    SDIO->CLKCR = (SDIO->CLKCR & ~SDIO_CLKCR_CLKDIV_Msk)
                | (hsd.Init.ClockDiv << SDIO_CLKCR_CLKDIV_Pos);
  }

  FIL fil;
  FRESULT fr = f_open(&fil, name, FA_READ);
  if (fr != FR_OK)
  {
    LCD_FillScreen(COLOR_BLACK);
    char buf[22];
    sprintf(buf, "Open err %d", (int)fr);
    LCD_DrawString(10, 50, buf, COLOR_RED, COLOR_BLACK);
    osDelay(2000);
    return;
  }

  /* 跳过 UTF-8 BOM */
  txt_skip_utf8_bom(&fil);

  uint32_t saved = txt_load_progress(name);
  if (saved > 0 && saved < f_size(&fil)) f_lseek(&fil, saved);

  uint32_t file_size = f_size(&fil);
  if (file_size == 0) { f_close(&fil); return; }

  /* 等待按键释放，避免从 FB_Run 长按进入后按键仍按下导致立即退出 */
  while (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET) osDelay(10);

  uint8_t exit_flag = 0;

  /* 进入页面循环前清除全屏，去除上一个画面的残留像素（如文件列表标题）*/
  LCD_FillScreen(COLOR_BLACK);

  while (!exit_flag)
  {
    /* 清除文本区域及顶部进度条区 */
    LCD_Fill(0, 0, LCD_WIDTH, 12 + TXT_LINES_PER_PAGE * TXT_ROW_HEIGHT,
             COLOR_BLACK);

    /* 读取并显示一页 */
    uint8_t rows = 0;
    char line[TXT_LINE_LEN];

    while (rows < TXT_LINES_PER_PAGE && f_gets(line, sizeof(line), &fil) != NULL)
    {
      /* remove trailing CR/LF */
      {
        char *p = line + strlen(line);
        while (p > line && (*(p-1)=='\r'||*(p-1)=='\n')) { *(p-1)='\0'; p--; }
      }

      /* long line wraps across multiple screen rows, no text lost */
      const char *cursor = line;
      while (rows < TXT_LINES_PER_PAGE && *cursor)
      {
        char row_buf[TXT_CHARS_PER_LINE + 1];
        uint32_t ci;
        for (ci = 0; ci < TXT_CHARS_PER_LINE && cursor[ci]; ci++)
          row_buf[ci] = cursor[ci];
        row_buf[ci] = '\0';

        LCD_DrawString(0, 12 + rows * TXT_ROW_HEIGHT, row_buf,
                      COLOR_WHITE, COLOR_BLACK);
        rows++;
        cursor += ci;
      }
    }

    uint32_t current_pos = f_tell(&fil);
    uint8_t is_end = (current_pos >= file_size);

    /* 保存进度 */
    txt_save_progress(name, is_end ? 0 : current_pos);

    /* 底部提示 */
    if (is_end)
    {
      LCD_DrawString(0, 120, "Hold=back re-read", COLOR_YELLOW, COLOR_BLACK);
      if (rows == 0) LCD_DrawString(40, 55, "- End -", COLOR_YELLOW, COLOR_BLACK);
    }
    else
    {
      uint8_t pct = (uint32_t)current_pos * 100 / file_size;
      char buf[6];
      sprintf(buf, "%d%%", pct);
      LCD_DrawString(LCD_WIDTH - 30, 0, buf, COLOR_CYAN, COLOR_BLACK);
    }

    /* === 自动翻页：每4秒翻一页 / 长按退出 === */
    uint32_t page_start = osKernelGetTickCount();
    uint8_t waiting = 1;
    while (waiting && !exit_flag)
    {
      if (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET)
      {
        uint32_t press_tick = osKernelGetTickCount();
        osDelay(5);                         /* 消抖 */
        if (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET)
        {
          /* 等按键释放，若按下 ≥500ms 则退出阅读器 */
          while (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET)
          {
            if (osKernelGetTickCount() - press_tick >= 500)
            {
              exit_flag = 1;
              waiting = 0;
              break;
            }
            osDelay(10);
          }
          /* 短按释放（<500ms）→ 翻到下一页 */
          osDelay(5);                       /* 二次消抖 */
          break;
        }
      }

      /* 4秒超时自动翻页 */
      if (osKernelGetTickCount() - page_start >= 4000)
      {
        waiting = 0;
        if (is_end) f_lseek(&fil, 0);
        break;
      }

      osDelay(20);
    }
  }

  f_close(&fil);
}

/* ===================== 文件列表渲染（精简ASCII） ===================== */
static void fb_render_filelist(uint8_t top, uint8_t sel)
{
  LCD_FillScreen(COLOR_BLACK);
  LCD_DrawString(0, 0, "SD File Browser", COLOR_CYAN, COLOR_BLACK);

  for (uint8_t i = 0; i < MAX_VISIBLE && (top + i) < file_cnt; i++)
  {
    uint8_t idx = top + i;
    uint8_t sel_flag = (idx == sel);
    uint16_t fy = 10 + i * 8;
    uint16_t fc = sel_flag ? COLOR_BLACK : COLOR_WHITE;
    uint16_t bg = sel_flag ? COLOR_YELLOW : COLOR_BLACK;

    char buf[22];
    buf[0] = sel_flag ? '>' : ' ';
    /* 复制文件名，过滤非ASCII字符（8.3名可能有OEM扩展字节）*/
    uint8_t di = 1;
    for (const char *p = file_list[idx]; *p && di < 21; p++)
    {
      uint8_t c = (uint8_t)*p;
      buf[di++] = (c >= 0x20 && c < 0x80) ? c : '.';
    }
    buf[di] = '\0';
    LCD_DrawString(0, fy, buf, fc, bg);
  }
  LCD_DrawString(0, 118, "Hold=open Short=next", COLOR_WHITE, COLOR_BLACK);
}

/* ===================== 初始化 ===================== */
void FB_Init(void)
{
  FRESULT res;
  DIR dir;
  FILINFO fno;

  /* 挂载 SD 卡 */
  res = f_mount(&SDFatFS, SDPath, 1);
  if (res != FR_OK) Error_Handler();

  /* 恢复 SDIO 时钟 */
  extern SD_HandleTypeDef hsd;
  SDIO->CLKCR = (SDIO->CLKCR & ~SDIO_CLKCR_CLKDIV_Msk)
              | (hsd.Init.ClockDiv << SDIO_CLKCR_CLKDIV_Pos);

  /* 初始化 LCD */
  LCD_Init();
  LCD_FillScreen(COLOR_BLACK);
  LCD_DrawString(0, 0, "SD File Browser", COLOR_CYAN, COLOR_BLACK);

  /* 列出 SD 卡根目录文件 */
  file_cnt = 0;
  if (f_opendir(&dir, "/") == FR_OK)
  {
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] && file_cnt < MAX_FILES)
    {
      if (!(fno.fattrib & AM_DIR))
      {
        strncpy(file_list[file_cnt], fno.fname, NAME_LEN - 1);
        file_list[file_cnt][NAME_LEN - 1] = '\0';
        file_cnt++;
      }
    }
    f_closedir(&dir);
  }

  if (file_cnt == 0)
  {
    LCD_DrawString(0, 20, "No files!", COLOR_RED, COLOR_BLACK);
    while (1) osDelay(1000);
  }
}

/* ===================== 主循环 ===================== */
void FB_Run(void)
{
  uint8_t sel = 0;
  uint8_t top = 0;
  uint8_t key_state = 0;
  uint32_t press_tick = 0;
  uint8_t need_refresh = 1;

  for (;;)
  {
    /* ========== 刷新文件列表（支持中文渲染） ========== */
    if (need_refresh)
    {
      fb_render_filelist(top, sel);
      need_refresh = 0;
    }

    /* ========== 按键检测 ========== */
    uint8_t pin = HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin);

    if (pin == GPIO_PIN_SET && key_state == 0)
    {
      key_state = 1;
      press_tick = osKernelGetTickCount();
    }
    else if (pin == GPIO_PIN_SET && key_state == 1)
    {
      if (osKernelGetTickCount() - press_tick >= 500)
      {
        /* 进入文件打开逻辑前先复位按键状态机，避免返回后卡死 */
        key_state = 0;
        need_refresh = 1;

        /* ======== 长按：打开文件 ======== */
        char *name = file_list[sel];
        char *ext = strrchr(name, '.');

        if (ext)
        {
          if (strcmp(ext, ".bmp") == 0 || strcmp(ext, ".BMP") == 0)
          {
            LCD_FillScreen(COLOR_BLACK);
            display_bmp(name);
            osDelay(2000);
          }
          else if (strcmp(ext, ".wav") == 0 || strcmp(ext, ".WAV") == 0)
          {
            LCD_FillScreen(COLOR_BLACK);
            {
              char *ext_pos = strrchr(name, '.');
              if (ext_pos)
              {
                char bmp_name[NAME_LEN];
                uint32_t stem_len = ext_pos - name;
                if (stem_len + 4 < NAME_LEN)
                {
                  memcpy(bmp_name, name, stem_len);
                  bmp_name[stem_len] = '\0';
                  strcat(bmp_name, ".bmp");
                  display_bmp(bmp_name);
                }
                else
                {
                  LCD_FillScreen(COLOR_BLACK);
                }
              }
              else
              {
                LCD_FillScreen(COLOR_BLACK);
              }
            }
            AUDIO_PlayWAV(name);
            osDelay(500);
          }
          else if (strcmp(ext, ".txt") == 0 || strcmp(ext, ".TXT") == 0)
          {
            display_txt_paged(name);
          }
          else
          {
            LCD_FillScreen(COLOR_BLACK);
            LCD_DrawString(10, 55, "Unsupported", COLOR_RED, COLOR_BLACK);
            osDelay(1500);
          }
        }

        /* 等待按键释放，避免文件操作返回后按键仍按下导致立即重新触发 */
        while (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET) osDelay(10);
      }
    }
    else if (pin == GPIO_PIN_RESET && key_state == 1)
    {
      key_state = 0;

      sel++;
      if (sel >= file_cnt) sel = 0;

      if (sel < top)
        top = sel;
      else if (sel >= top + MAX_VISIBLE)
        top = sel - MAX_VISIBLE + 1;

      need_refresh = 1;
    }
    else if (pin == GPIO_PIN_RESET && key_state == 2)
    {
      key_state = 0;
    }
    else if (pin == GPIO_PIN_SET && key_state == 2)
    {
      /* key_state=2时按键仍按下：来自continue或文件操作返回后用户再次按键 */
      /* 什么都不做，等待释放后自动复位到key_state=0 */
    }

    osDelay(30);
  }
}
