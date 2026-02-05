#define TFT_CONFIG_ONLY
#include "common.h"
#undef TFT_CONFIG_ONLY

#include "st7735.h"
#include "ch32v003fun.h"
#include "fontk_8x8.h"

// CH32V003 pin definitions (matches legacy driver behavior)
#define PIN_RESET 7  // PC7
#define PIN_DC    0  // PD0
#ifndef ST7735_NO_CS
#define PIN_CS 3  // PC3
#endif
#define SPI_SCLK 5  // PC5
#define SPI_MOSI 6  // PC6

#define DATA_MODE()    (GPIOD->BSHR |= 1 << PIN_DC)
#define COMMAND_MODE() (GPIOD->BCR |= 1 << PIN_DC)
#define RESET_HIGH()   (GPIOC->BSHR |= 1 << PIN_RESET)
#define RESET_LOW()    (GPIOC->BCR |= 1 << PIN_RESET)
#ifndef ST7735_NO_CS
#define START_WRITE() (GPIOC->BCR |= 1 << PIN_CS)
#define END_WRITE()   (GPIOC->BSHR |= 1 << PIN_CS)
#else
#define START_WRITE()
#define END_WRITE()
#endif

// Delays (ms)
#define ST7735_RST_DELAY     60
#define ST7735_SLPOUT_DELAY 120

// Command set
#define ST7735_SLPIN   0x10
#define ST7735_SLPOUT  0x11
#define ST7735_PTLON   0x12
#define ST7735_NORON   0x13
#define ST7735_INVOFF  0x20
#define ST7735_INVON   0x21
#define ST7735_GAMSET  0x26
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_PLTAR   0x30
#define ST7735_TEOFF   0x34
#define ST7735_TEON    0x35
#define ST7735_MADCTL  0x36
#define ST7735_IDMOFF  0x38
#define ST7735_IDMON   0x39
#define ST7735_COLMOD  0x3A

#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

// MADCTL parameters
#define ST7735_MADCTL_MH  0x04
#define ST7735_MADCTL_RGB 0x00
#define ST7735_MADCTL_BGR 0x08
#define ST7735_MADCTL_ML  0x10
#define ST7735_MADCTL_MV  0x20
#define ST7735_MADCTL_MX  0x40
#define ST7735_MADCTL_MY  0x80

#define ST7735_COLMOD_16_BPP 0x05

#define FONT_WIDTH  8
#define FONT_HEIGHT 8

static uint16_t _cursor_x = 0;
static uint16_t _cursor_y = 0;
static uint16_t _color = WHITE;
static uint16_t _bg_color = BLACK;
static uint8_t  _buffer[ST7735_WIDTH * 2] = {0};

// --- Low-level SPI/DMA helpers ---

static void spi_init(void)
{
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_SPI1;

    GPIOC->CFGLR &= ~(0xf << (PIN_RESET << 2));
    GPIOC->CFGLR |= (GPIO_CNF_OUT_PP | GPIO_Speed_50MHz) << (PIN_RESET << 2);

    GPIOD->CFGLR &= ~(0xf << (PIN_DC << 2));
    GPIOD->CFGLR |= (GPIO_CNF_OUT_PP | GPIO_Speed_50MHz) << (PIN_DC << 2);

#ifndef ST7735_NO_CS
    GPIOC->CFGLR &= ~(0xf << (PIN_CS << 2));
    GPIOC->CFGLR |= (GPIO_CNF_OUT_PP | GPIO_Speed_50MHz) << (PIN_CS << 2);
#endif

    GPIOC->CFGLR &= ~(0xf << (SPI_SCLK << 2));
    GPIOC->CFGLR |= (GPIO_CNF_OUT_PP_AF | GPIO_Speed_50MHz) << (SPI_SCLK << 2);

    GPIOC->CFGLR &= ~(0xf << (SPI_MOSI << 2));
    GPIOC->CFGLR |= (GPIO_CNF_OUT_PP_AF | GPIO_Speed_50MHz) << (SPI_MOSI << 2);

    SPI1->CTLR1 = SPI_CPHA_1Edge
                  | SPI_CPOL_Low
                  | SPI_Mode_Master
                  | SPI_BaudRatePrescaler_2
                  | SPI_FirstBit_MSB
                  | SPI_NSS_Soft
                  | SPI_DataSize_8b
                  | SPI_Direction_1Line_Tx;
    SPI1->CRCR = 7;
    SPI1->CTLR2 |= SPI_I2S_DMAReq_Tx;
    SPI1->CTLR1 |= CTLR1_SPE_Set;

    RCC->AHBPCENR |= RCC_AHBPeriph_DMA1;
    DMA1_Channel3->CFGR = DMA_DIR_PeripheralDST
                          | DMA_Mode_Circular
                          | DMA_PeripheralInc_Disable
                          | DMA_MemoryInc_Enable
                          | DMA_PeripheralDataSize_Byte
                          | DMA_MemoryDataSize_Byte
                          | DMA_Priority_VeryHigh
                          | DMA_M2M_Disable;
    DMA1_Channel3->PADDR = (uint32_t)&SPI1->DATAR;
}

static void spi_write_dma_repeat(const uint8_t* buffer, uint16_t size, uint16_t repeat)
{
    // Send the same buffer via DMA, repeating without reprogramming source.
    DMA1_Channel3->MADDR = (uint32_t)buffer;
    DMA1_Channel3->CNTR = size;
    DMA1_Channel3->CFGR |= DMA_CFGR1_EN;

    while (repeat--)
    {
        DMA1->INTFCR = DMA1_FLAG_TC3;
        while (!(DMA1->INTFR & DMA1_FLAG_TC3))
            ;
    }

    DMA1_Channel3->CFGR &= ~DMA_CFGR1_EN;
}

static inline void spi_write(uint8_t data)
{
    // Blocking single-byte SPI write.
    SPI1->DATAR = data;
    while (!(SPI1->STATR & SPI_STATR_TXE))
        ;
}

static inline void tft_write_cmd(uint8_t cmd)
{
    // Command phase: DC low, single-byte SPI write.
    COMMAND_MODE();
    spi_write(cmd);
}

static inline void tft_write_data(uint8_t data)
{
    // Data phase: DC high, single-byte SPI write.
    DATA_MODE();
    spi_write(data);
}

static inline void tft_write_data16(uint16_t data)
{
    // Data phase: DC high, transmit MSB then LSB.
    DATA_MODE();
    spi_write(data >> 8);
    spi_write(data);
}

static void tft_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    // Set drawing window (inclusive) for subsequent RAM writes.
    tft_write_cmd(ST7735_CASET);
    tft_write_data16(x0);
    tft_write_data16(x1);
    tft_write_cmd(ST7735_RASET);
    tft_write_data16(y0);
    tft_write_data16(y1);
    tft_write_cmd(ST7735_RAMWR);
}

// --- Public API ---
void st7735_init(void)
{
    spi_init();

    RESET_LOW();
    Delay_Ms(ST7735_RST_DELAY);
    RESET_HIGH();
    Delay_Ms(ST7735_RST_DELAY);

    START_WRITE();

    tft_write_cmd(ST7735_SLPOUT);
    Delay_Ms(ST7735_SLPOUT_DELAY);

    tft_write_cmd(ST7735_MADCTL);
    tft_write_data((uint8_t)(ST7735_MADCTL_MX | ST7735_MADCTL_MV | ST7735_MADCTL_BGR));

    tft_write_cmd(ST7735_COLMOD);
    tft_write_data(ST7735_COLMOD_16_BPP);

    {
        uint8_t gamma_p[] = {0x09, 0x16, 0x09, 0x20, 0x21, 0x1B, 0x13, 0x19,
                             0x17, 0x15, 0x1E, 0x2B, 0x04, 0x05, 0x02, 0x0E};
        tft_write_cmd(ST7735_GMCTRP1);
        DATA_MODE();
        spi_write_dma_repeat(gamma_p, 16, 1);
        Delay_Ms(10);

        uint8_t gamma_n[] = {0x0B, 0x14, 0x08, 0x1E, 0x22, 0x1D, 0x18, 0x1E,
                             0x1B, 0x1A, 0x24, 0x2B, 0x06, 0x06, 0x02, 0x0F};
        tft_write_cmd(ST7735_GMCTRN1);
        DATA_MODE();
        spi_write_dma_repeat(gamma_n, 16, 1);
        Delay_Ms(10);
    }

    tft_write_cmd(ST7735_INVOFF);

    tft_write_cmd(ST7735_NORON);
    Delay_Ms(10);

    tft_write_cmd(ST7735_DISPON);
    Delay_Ms(10);

    END_WRITE();
}

void st7735_set_cursor(uint16_t x, uint16_t y)
{
    _cursor_x = x + ST7735_X_OFFSET;
    _cursor_y = y + ST7735_Y_OFFSET;
}

void st7735_set_color(uint16_t color)
{
    _color = color;
}

void st7735_set_background_color(uint16_t color)
{
    _bg_color = color;
}

void st7735_print_char(char c, uint8_t font_scale)
{
    if (font_scale < 1) font_scale = 1;
    if (font_scale > 2) font_scale = 2;

    const uint8_t* start = &font[((uint8_t)c) << 3];
    const uint8_t s = font_scale;

    const uint16_t out_w = FONT_WIDTH * s;

    START_WRITE();
    for (uint8_t i = 0; i < FONT_HEIGHT; i++)
    {
        const uint8_t colbits = (uint8_t)(1U << i);
        for (uint8_t sy = 0; sy < s; sy++)
        {
            uint16_t sz = 0;
            for (uint8_t j = 0; j < FONT_WIDTH; j++)
            {
                const uint8_t on = (start[j] & colbits) ? 1 : 0;
                const uint16_t color = on ? _color : _bg_color;
                for (uint8_t sx = 0; sx < s; sx++)
                {
                    _buffer[sz++] = (uint8_t)(color >> 8);
                    _buffer[sz++] = (uint8_t)(color & 0xFF);
                }
            }
            tft_set_window(_cursor_x, _cursor_y + (i * s + sy),
                           _cursor_x + out_w - 1, _cursor_y + (i * s + sy));
            DATA_MODE();
            spi_write_dma_repeat(_buffer, sz, 1);
        }
    }
    END_WRITE();
}

void st7735_print(const char* str, uint8_t scale)
{
    while (*str)
    {
        st7735_print_char(*str++, scale);
        _cursor_x += (FONT_WIDTH - 2) * scale;
    }
}

void st7735_print_number(int32_t num, uint16_t width)
{
    static char str[12];
    uint8_t position = 11;
    uint8_t negative = 0;
    uint16_t num_width = 0;

    if (num < 0)
    {
        negative = 1;
        num = -num;
    }

    str[position] = '\0';
    while (num)
    {
        str[--position] = num % 10 + '0';
        num /= 10;
    }

    if (position == 11)
    {
        str[--position] = '0';
    }

    if (negative)
    {
        str[--position] = '-';
    }

    num_width = (11 - position) * (FONT_WIDTH + 1) - 1;
    if (width > num_width)
    {
        _cursor_x += width - num_width;
    }

    st7735_print(&str[position], 1);
}

void st7735_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    x += ST7735_X_OFFSET;
    y += ST7735_Y_OFFSET;
    START_WRITE();
    tft_set_window(x, y, x, y);
    tft_write_data16(color);
    END_WRITE();
}

void st7735_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    x += ST7735_X_OFFSET;
    y += ST7735_Y_OFFSET;

    uint16_t sz = 0;
    for (uint16_t i = 0; i < width; i++)
    {
        _buffer[sz++] = color >> 8;
        _buffer[sz++] = color;
    }

    START_WRITE();
    tft_set_window(x, y, x + width - 1, y + height - 1);
    DATA_MODE();
    spi_write_dma_repeat(_buffer, sz, height);
    END_WRITE();
}

void st7735_draw_bitmap(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* bitmap)
{
    x += ST7735_X_OFFSET;
    y += ST7735_Y_OFFSET;
    START_WRITE();
    tft_set_window(x, y, x + width - 1, y + height - 1);
    DATA_MODE();
    spi_write_dma_repeat(bitmap, width * height << 1, 1);
    END_WRITE();
}

static void _tft_draw_fast_v_line(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    x += ST7735_X_OFFSET;
    y += ST7735_Y_OFFSET;

    uint16_t sz = 0;
    for (int16_t j = 0; j < h; j++)
    {
        _buffer[sz++] = color >> 8;
        _buffer[sz++] = color;
    }

    START_WRITE();
    tft_set_window(x, y, x, y + h - 1);
    DATA_MODE();
    spi_write_dma_repeat(_buffer, sz, 1);
    END_WRITE();
}

static void _tft_draw_fast_h_line(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    x += ST7735_X_OFFSET;
    y += ST7735_Y_OFFSET;

    uint16_t sz = 0;
    for (int16_t j = 0; j < w; j++)
    {
        _buffer[sz++] = color >> 8;
        _buffer[sz++] = color;
    }

    START_WRITE();
    tft_set_window(x, y, x + w - 1, y);
    DATA_MODE();
    spi_write_dma_repeat(_buffer, sz, 1);
    END_WRITE();
}

#define _diff(a, b) ((a > b) ? (a - b) : (b - a))
#define _swap_int16_t(a, b) \
    {                       \
        int16_t t = a;      \
        a = b;              \
        b = t;              \
    }

static void _tft_draw_line_bresenham(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    uint8_t steep = _diff(y1, y0) > _diff(x1, x0);
    if (steep)
    {
        _swap_int16_t(x0, y0);
        _swap_int16_t(x1, y1);
    }

    if (x0 > x1)
    {
        _swap_int16_t(x0, x1);
        _swap_int16_t(y0, y1);
    }

    int16_t dx = x1 - x0;
    int16_t dy = _diff(y1, y0);
    int16_t err = dx >> 1;
    int16_t step = (y0 < y1) ? 1 : -1;

    for (; x0 <= x1; x0++)
    {
        if (steep)
        {
            st7735_draw_pixel(y0, x0, color);
        }
        else
        {
            st7735_draw_pixel(x0, y0, color);
        }
        err -= dy;
        if (err < 0)
        {
            err += dx;
            y0 += step;
        }
    }
}

void st7735_draw_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    _tft_draw_fast_h_line(x, y, width, color);
    _tft_draw_fast_h_line(x, y + height - 1, width, color);
    _tft_draw_fast_v_line(x, y, height, color);
    _tft_draw_fast_v_line(x + width - 1, y, height, color);
}

void st7735_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    if (x0 == x1)
    {
        if (y0 > y1)
        {
            _swap_int16_t(y0, y1);
        }
        _tft_draw_fast_v_line(x0, y0, y1 - y0 + 1, color);
    }
    else if (y0 == y1)
    {
        if (x0 > x1)
        {
            _swap_int16_t(x0, x1);
        }
        _tft_draw_fast_h_line(x0, y0, x1 - x0 + 1, color);
    }
    else
    {
        _tft_draw_line_bresenham(x0, y0, x1, y1, color);
    }
}
