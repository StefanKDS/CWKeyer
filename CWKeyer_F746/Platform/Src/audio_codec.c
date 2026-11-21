/**
 * @file    audio_codec.c
 * @brief   SAI2 + WM8994 full duplex driver (own implementation, the BSP
 *          audio driver is not used because it does not support
 *          line in + headphone simultaneously in a simple way).
 *
 * SAI2 block A: master TX, generates MCLK/BCLK/FS, slots 0+2 (L/R)
 * SAI2 block B: synchronous slave RX, slots 0+2
 * DMA2 stream 4 ch 3 (TX), DMA2 stream 7 ch 0 (RX), both circular.
 *
 * Processing happens in the RX callbacks: when half n of the RX buffer
 * is complete, half n of the TX buffer has already been sent (TX runs
 * ahead because of its FIFO), so it can be refilled.
 */
#include "audio_codec.h"
#include "app.h"
#include "board.h"
#include "stm32746g_discovery.h"
#include "wm8994.h"
#include <string.h>

SAI_HandleTypeDef  hsai_tx;
SAI_HandleTypeDef  hsai_rx;
DMA_HandleTypeDef  hdma_sai_tx;
DMA_HandleTypeDef  hdma_sai_rx;

/* buffers in non-cacheable SRAM2 (see MPU config + linker script) */
#define BUF_SAMPLES (2u * AUDIO_BLOCK_FRAMES * 2u)          /* 2 halves, stereo */
static int16_t s_tx[BUF_SAMPLES] __attribute__((section(".dma_buffers"), aligned(32)));
static int16_t s_rx[BUF_SAMPLES] __attribute__((section(".dma_buffers"), aligned(32)));

static bool   s_ok;
static int8_t s_gain_db;

/* -------------------------------------------------------------------- */

static void sai_clock_config(void)
{
    /* 49.142 MHz SAI clock for 8/16/32/48/96 kHz (as in the ST BSP) */
    RCC_PeriphCLKInitTypeDef clk;
    HAL_RCCEx_GetPeriphCLKConfig(&clk);
    clk.PeriphClockSelection = RCC_PERIPHCLK_SAI2;
    clk.Sai2ClockSelection   = RCC_SAI2CLKSOURCE_PLLI2S;
    clk.PLLI2S.PLLI2SN       = 344;
    clk.PLLI2S.PLLI2SQ       = 7;
    clk.PLLI2SDivQ           = 1;
    HAL_RCCEx_PeriphCLKConfig(&clk);
}

void HAL_SAI_MspInit(SAI_HandleTypeDef *hsai)
{
    GPIO_InitTypeDef g = { 0 };

    __HAL_RCC_SAI2_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF10_SAI2;

    if (hsai->Instance == SAI2_Block_A)
    {
        /* PI4 MCLK_A, PI5 SCK_A, PI6 SD_A, PI7 FS_A */
        g.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
        HAL_GPIO_Init(GPIOI, &g);

        hdma_sai_tx.Instance                 = DMA2_Stream4;
        hdma_sai_tx.Init.Channel             = DMA_CHANNEL_3;
        hdma_sai_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma_sai_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_sai_tx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_sai_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        hdma_sai_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
        hdma_sai_tx.Init.Mode                = DMA_CIRCULAR;
        hdma_sai_tx.Init.Priority            = DMA_PRIORITY_HIGH;
        hdma_sai_tx.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
        hdma_sai_tx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
        hdma_sai_tx.Init.MemBurst            = DMA_MBURST_SINGLE;
        hdma_sai_tx.Init.PeriphBurst         = DMA_PBURST_SINGLE;
        __HAL_LINKDMA(hsai, hdmatx, hdma_sai_tx);
        HAL_DMA_DeInit(&hdma_sai_tx);
        HAL_DMA_Init(&hdma_sai_tx);
        HAL_NVIC_SetPriority(DMA2_Stream4_IRQn, IRQ_PRIO_AUDIO, 0);
        HAL_NVIC_EnableIRQ(DMA2_Stream4_IRQn);
    }
    else if (hsai->Instance == SAI2_Block_B)
    {
        /* PG10 SD_B */
        g.Pin = GPIO_PIN_10;
        HAL_GPIO_Init(GPIOG, &g);

        hdma_sai_rx.Instance                 = DMA2_Stream7;
        hdma_sai_rx.Init.Channel             = DMA_CHANNEL_0;
        hdma_sai_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
        hdma_sai_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma_sai_rx.Init.MemInc              = DMA_MINC_ENABLE;
        hdma_sai_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        hdma_sai_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
        hdma_sai_rx.Init.Mode                = DMA_CIRCULAR;
        hdma_sai_rx.Init.Priority            = DMA_PRIORITY_HIGH;
        hdma_sai_rx.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
        hdma_sai_rx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
        hdma_sai_rx.Init.MemBurst            = DMA_MBURST_SINGLE;
        hdma_sai_rx.Init.PeriphBurst         = DMA_PBURST_SINGLE;
        __HAL_LINKDMA(hsai, hdmarx, hdma_sai_rx);
        HAL_DMA_DeInit(&hdma_sai_rx);
        HAL_DMA_Init(&hdma_sai_rx);
        HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, IRQ_PRIO_AUDIO, 0);
        HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);
    }
}

static void sai_common(SAI_HandleTypeDef *h, uint32_t fs)
{
    h->Init.AudioFrequency    = fs;
    h->Init.NoDivider         = SAI_MASTERDIVIDER_ENABLE;
    h->Init.Protocol          = SAI_FREE_PROTOCOL;
    h->Init.DataSize          = SAI_DATASIZE_16;
    h->Init.FirstBit          = SAI_FIRSTBIT_MSB;
    h->Init.ClockStrobing     = SAI_CLOCKSTROBING_RISINGEDGE;
    h->Init.FIFOThreshold     = SAI_FIFOTHRESHOLD_1QF;
    h->Init.SynchroExt        = SAI_SYNCEXT_DISABLE;
    h->Init.CompandingMode    = SAI_NOCOMPANDING;
    h->Init.TriState          = SAI_OUTPUT_NOTRELEASED;
    h->Init.Mckdiv            = 0;

    /* 64 bit frame, 4 slots of 16 bit, codec in I2S mode: slot 0 = left,
       slot 2 = right (AIF1 timeslot 0) */
    h->FrameInit.FrameLength       = 64;
    h->FrameInit.ActiveFrameLength = 32;
    h->FrameInit.FSDefinition      = SAI_FS_CHANNEL_IDENTIFICATION;
    h->FrameInit.FSPolarity        = SAI_FS_ACTIVE_LOW;
    h->FrameInit.FSOffset          = SAI_FS_BEFOREFIRSTBIT;

    h->SlotInit.FirstBitOffset = 0;
    h->SlotInit.SlotSize       = SAI_SLOTSIZE_DATASIZE;
    h->SlotInit.SlotNumber     = 4;
    h->SlotInit.SlotActive     = SAI_SLOTACTIVE_0 | SAI_SLOTACTIVE_2;
}

static void codec_write(uint16_t reg, uint16_t val)
{
    AUDIO_IO_Write(AUDIO_I2C_ADDRESS, reg, val);
}

bool audio_codec_init(uint32_t fs)
{
    memset(s_tx, 0, sizeof(s_tx));
    memset(s_rx, 0, sizeof(s_rx));

    sai_clock_config();

    /* block A: master transmitter */
    hsai_tx.Instance = SAI2_Block_A;
    __HAL_SAI_DISABLE(&hsai_tx);
    sai_common(&hsai_tx, fs);
    hsai_tx.Init.AudioMode   = SAI_MODEMASTER_TX;
    hsai_tx.Init.Synchro     = SAI_ASYNCHRONOUS;
    hsai_tx.Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
    if (HAL_SAI_Init(&hsai_tx) != HAL_OK)
        return false;

    /* block B: synchronous slave receiver */
    hsai_rx.Instance = SAI2_Block_B;
    __HAL_SAI_DISABLE(&hsai_rx);
    sai_common(&hsai_rx, fs);
    hsai_rx.Init.AudioMode   = SAI_MODESLAVE_RX;
    hsai_rx.Init.Synchro     = SAI_SYNCHRONOUS;
    hsai_rx.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
    if (HAL_SAI_Init(&hsai_rx) != HAL_OK)
        return false;

    /* MCLK must run before the codec is configured */
    __HAL_SAI_ENABLE(&hsai_tx);

    /* codec: line in 1 + headphone, slave, I2S 16 bit */
    if (wm8994_drv.ReadID(AUDIO_I2C_ADDRESS) != WM8994_ID)
        return false;
    wm8994_drv.Reset(AUDIO_I2C_ADDRESS);
    if (wm8994_drv.Init(AUDIO_I2C_ADDRESS, INPUT_DEVICE_INPUT_LINE_1 | OUTPUT_DEVICE_HEADPHONE, 80, fs) != 0)
        return false;

    /* headphone volume fixed (-6 dB), volume is done in software */
    codec_write(0x1C, 0x0100 | 0x0040 | 0x0033);   /* HPOUT1L: VU, unmute, -6 dB */
    codec_write(0x1D, 0x0100 | 0x0040 | 0x0033);   /* HPOUT1R                    */
    /* AIF1 ADC1 digital volume 0 dB */
    codec_write(0x400, 0x0100 | 0x00C0);
    codec_write(0x401, 0x0100 | 0x00C0);
    audio_codec_set_input_gain(s_gain_db);

    wm8994_drv.Play(AUDIO_I2C_ADDRESS, NULL, 0);   /* unmute */

    /* start DMA: receiver first (waits for clocks), then transmitter */
    if (HAL_SAI_Receive_DMA(&hsai_rx, (uint8_t *)s_rx, BUF_SAMPLES) != HAL_OK)
        return false;
    if (HAL_SAI_Transmit_DMA(&hsai_tx, (uint8_t *)s_tx, BUF_SAMPLES) != HAL_OK)
        return false;

    s_ok = true;
    return true;
}

bool audio_codec_ok(void)
{
    return s_ok;
}

void audio_codec_set_input_gain(int8_t db)
{
    s_gain_db = db;
    if (!s_ok && hsai_tx.State == HAL_SAI_STATE_RESET)
        return;                                       /* codec not up yet */
    /* IN1L/IN1R PGA: -16.5 dB .. +30 dB in 1.5 dB steps, 0x0B = 0 dB */
    int v = 0x0B + (db * 2) / 3;
    if (v < 0) v = 0;
    if (v > 0x1F) v = 0x1F;
    codec_write(0x18, (uint16_t)(0x0100 | v));        /* VU, unmuted */
    codec_write(0x1A, (uint16_t)(0x0100 | v));
}

/* -------------------------------------------------------------------- */

void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai == &hsai_rx)
        app_audio_block(&s_rx[0], &s_tx[0], AUDIO_BLOCK_FRAMES);
}

void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
    if (hsai == &hsai_rx)
        app_audio_block(&s_rx[BUF_SAMPLES / 2], &s_tx[BUF_SAMPLES / 2], AUDIO_BLOCK_FRAMES);
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
    (void)hsai;   /* over/underrun: DMA keeps running in circular mode */
}
