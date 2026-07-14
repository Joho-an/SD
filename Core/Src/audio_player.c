#include "audio_player.h"
#include "i2s.h"
#include "fatfs.h"
#include "lcd_st7735.h"
#include "cmsis_os.h"
#include <string.h>

static volatile uint8_t dma_done;
/* 双缓冲（Ping-Pong Buffer），消除DMA启停间隙 */
#define BUF_SAMPLES  16384                     /* 每缓冲 16384 样本 */
static uint16_t abuf0[BUF_SAMPLES];
static uint16_t abuf1[BUF_SAMPLES];

/* 默认音量：最大值的 1/10 */
#define VOLUME_DIV  50

/* 对 buf 中 count 个 16-bit 样本应用软件音量 */
static void apply_volume(uint16_t *buf, uint32_t count)
{
  for (uint32_t i = 0; i < count; i++)
  {
    int32_t s = (int16_t)buf[i];   /* 转为有符号 16-bit PCM */
    s /= VOLUME_DIV;
    buf[i] = (uint16_t)(int16_t)s;
  }
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
  if (hi2s->Instance == SPI3) dma_done = 1;
}

void AUDIO_PlayWAV(const char *filename)
{
  /* 恢复 SDIO 时钟（避免 BMP 后时钟改变导致读写失败）*/
  {
    extern SD_HandleTypeDef hsd;
    SDIO->CLKCR = (SDIO->CLKCR & ~SDIO_CLKCR_CLKDIV_Msk)
                | (hsd.Init.ClockDiv << SDIO_CLKCR_CLKDIV_Pos);
  }

  FIL wav;
  UINT br;
  uint8_t hdr[44];
  FRESULT res;

  res = f_open(&wav, filename, FA_READ);
  if (res != FR_OK) return;

  f_read(&wav, hdr, 44, &br);
  if (br != 44) { f_close(&wav); return; }
  if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr+8, "WAVE", 4) != 0)
  { f_close(&wav); return; }

  uint16_t fmt      = hdr[20] | (hdr[21] << 8);
  uint16_t channels = hdr[22] | (hdr[23] << 8);
  uint16_t bits     = hdr[34] | (hdr[35] << 8);
  uint32_t rate     = hdr[24]|(hdr[25]<<8)|(hdr[26]<<16)|(hdr[27]<<24);

  if (fmt != 1 || bits != 16) { f_close(&wav); return; }

  /* 搜索 data chunk */
  uint32_t pos = 12, data_off = 0, data_sz = 0;
  while (pos < f_size(&wav) - 8)
  {
    uint8_t cid[4], csz[4];
    f_lseek(&wav, pos);
    f_read(&wav, cid, 4, &br);
    f_read(&wav, csz, 4, &br);
    uint32_t csz_val = csz[0]|(csz[1]<<8)|(csz[2]<<16)|(csz[3]<<24);
    if (memcmp(cid, "data", 4) == 0)
    { data_off = pos+8; data_sz = csz_val; break; }
    if (csz_val == 0) break;
    pos += 8 + csz_val;
  }
  if (!data_off) { f_close(&wav); return; }

  /* 配置 I2S */
  HAL_I2S_DeInit(&hi2s3);
  hi2s3.Init.AudioFreq = rate;
  HAL_I2S_Init(&hi2s3);

  f_lseek(&wav, data_off);
  uint32_t remain = data_sz;

  /* 每块从 SD 卡读入的字节数 */
  uint32_t chunk = (channels == 1) ? BUF_SAMPLES : BUF_SAMPLES * 2;
  /* 发送给 DMA 的 half-word 数（立体声样本数） */
  uint32_t hw    = BUF_SAMPLES;

  /* ----- 双缓冲（Ping-Pong）播放 ----- */

  /* 第一步：填充缓冲0，启动首次DMA传输 */
  {
    uint32_t n = (remain >= chunk) ? chunk : remain;
    memset(abuf0, 0, sizeof(abuf0));
    f_read(&wav, abuf0, n, &br);
    remain -= (br <= remain) ? br : remain;

    if (channels == 1)
    {
      for (int32_t i = (hw / 2) - 1; i >= 0; i--)
      {
        uint16_t s = abuf0[i];
        abuf0[i * 2]     = s;
        abuf0[i * 2 + 1] = s;
      }
    }

    apply_volume(abuf0, hw);

    dma_done = 0;
    HAL_I2S_Transmit_DMA(&hi2s3, abuf0, hw);
  }

  uint8_t active = 0;  /* 0=abuf0正在播放, 1=abuf1正在播放 */

  while (remain > 0)
  {
    /* 填充下一个缓冲（DMA正在播放当前缓冲，两者互不干扰） */
    uint16_t *next_buf = (active == 0) ? abuf1 : abuf0;
    uint32_t n = (remain >= chunk) ? chunk : remain;
    memset(next_buf, 0, sizeof(abuf0));
    f_read(&wav, next_buf, n, &br);
    remain -= (br <= remain) ? br : remain;

    if (channels == 1)
    {
      for (int32_t i = (hw / 2) - 1; i >= 0; i--)
      {
        uint16_t s = next_buf[i];
        next_buf[i * 2]     = s;
        next_buf[i * 2 + 1] = s;
      }
    }

    apply_volume(next_buf, hw);

    /* 等待当前DMA传输完成 */
    {
      uint32_t wt = osKernelGetTickCount();
      while (!dma_done)
      {
        if (osKernelGetTickCount() - wt > 2000) break;
        osDelay(1);
      }
    }

    /* 立即启动下一个缓冲（无缝衔接，消除块间间隙） */
    dma_done = 0;
    HAL_I2S_Transmit_DMA(&hi2s3, next_buf, hw);
    active = !active;

    /* 按键停止 */
    if (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET)
    {
      osDelay(300);
      if (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET)
      { HAL_I2S_DMAStop(&hi2s3); break; }
    }
  }

  /* 等待最后一块播放完毕 */
  if (remain == 0)
  {
    uint32_t wt = osKernelGetTickCount();
    while (!dma_done)
    {
      if (osKernelGetTickCount() - wt > 2000) break;
      osDelay(1);
    }
  }

  HAL_I2S_DMAStop(&hi2s3);
  f_close(&wav);
}
