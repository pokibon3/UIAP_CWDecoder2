#define TFT_CONFIG_ONLY
#include "common.h"
#undef TFT_CONFIG_ONLY

#include "st7789.h"
#include "ch32v003fun.h"
#include "fontk_8x8.h"

// CH32V003 pin definitions (same wiring as ST7735)
#define PIN_RESET 7  // PC7
#define PIN_DC    0  // PD0
#ifndef ST7789_NO_CS
#define PIN_CS 3  // PC3
#endif
#define SPI_SCLK 5  // PC5
#define SPI_MOSI 6  // PC6

#define DATA_MODE()    (GPIOD->BSHR |= 1 << PIN_DC)
#define COMMAND_MODE() (GPIOD->BCR |= 1 << PIN_DC)
#define RESET_HIGH()   (GPIOC->BSHR |= 1 << PIN_RESET)
#define RESET_LOW()    (GPIOC->BCR |= 1 << PIN_RESET)
#ifndef ST7789_NO_CS
#define START_WRITE() (GPIOC->BCR |= 1 << PIN_CS)
#define END_WRITE()   (GPIOC->BSHR |= 1 << PIN_CS)
#else
#define START_WRITE()
#define END_WRITE()
#endif

// Delays (ms)
#define ST7789_RST_DELAY     120
#define ST7789_SLPOUT_DELAY  120

// Command set
#define ST7789_SLPOUT  0x11
#define ST7789_DISPON  0x29
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C
#define ST7789_MADCTL  0x36
#define ST7789_COLMOD  0x3A
#define ST7789_INVON   0x21
#define ST7789_INVOFF  0x20
#define ST7789_VSCRDEF 0x33
#define ST7789_VSCRSADD 0x37

// MADCTL parameters
#define ST7789_MADCTL_RGB 0x00
#define ST7789_MADCTL_BGR 0x08
#define ST7789_MADCTL_MV  0x20
#define ST7789_MADCTL_MX  0x40
#define ST7789_MADCTL_MY  0x80

#define ST7789_COLMOD_16_BPP 0x55

#define FONT_WIDTH  8
#define FONT_HEIGHT 8

static uint16_t _cursor_x = 0;
static uint16_t _cursor_y = 0;
static uint16_t _color = WHITE;
static uint16_t _bg_color = BLACK;
static uint8_t  _buffer[ST7789_WIDTH * 2] = {0};

// --- Low-level SPI/DMA helpers ---

static void spi_init(void)
{
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_SPI1;

    GPIOC->CFGLR &= ~(0xf << (PIN_RESET << 2));
    GPIOC->CFGLR |= (GPIO_CNF_OUT_PP | GPIO_Speed_50MHz) << (PIN_RESET << 2);

    GPIOD->CFGLR &= ~(0xf << (PIN_DC << 2));
    GPIOD->CFGLR |= (GPIO_CNF_OUT_PP | GPIO_Speed_50MHz) << (PIN_DC << 2);

#ifndef ST7789_NO_CS
    GPIOC->CFGLR &= ~(0xf << (PIN_CS << 2));
    GPIOC->CFGLR |= (GPIO_CNF_OUT_PP | GPIO_Speed_50MHz) << (PIN_CS << 2);
#endif

    GPIOC->CFGLR &= ~(0xf << (SPI_SCLK << 2));
    GPIOC->CFGLR |= (GPIO_CNF_OUT_PP_AF | GPIO_Speed_50MHz) << (SPI_SCLK << 2);

    GPIOC->CFGLR &= ~(0xf << (SPI_MOSI << 2));
    GPIOC->CFGLR |= (GPIO_CNF_OUT_PP_AF | GPIO_Speed_50MHz) << (SPI_MOSI << 2);

    // CPOL=1, CPHA=1 (matches known-working timing for ST7789)
    SPI1->CTLR1 = SPI_CPHA_2Edge
                  | SPI_CPOL_High
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
    SPI1->DATAR = data;
    while (!(SPI1->STATR & SPI_STATR_TXE))
        ;
}

static inline void tft_write_cmd(uint8_t cmd)
{
    COMMAND_MODE();
    spi_write(cmd);
}

static inline void tft_write_data(uint8_t data)
{
    DATA_MODE();
    spi_write(data);
}

static inline void tft_write_data16(uint16_t data)
{
    DATA_MODE();
    spi_write(data >> 8);
    spi_write(data);
}

static void tft_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    tft_write_cmd(ST7789_CASET);
    tft_write_data16(x0);
    tft_write_data16(x1);
    tft_write_cmd(ST7789_RASET);
    tft_write_data16(y0);
    tft_write_data16(y1);
    tft_write_cmd(ST7789_RAMWR);
}

// --- Public API ---
void st7789_init(void)
{
    spi_init();

    RESET_HIGH();
    Delay_Ms(1);
    RESET_LOW();
    Delay_Ms(1);
    RESET_HIGH();
    Delay_Ms(ST7789_RST_DELAY);

    START_WRITE();

    tft_write_cmd(ST7789_SLPOUT);
    Delay_Ms(ST7789_SLPOUT_DELAY);

    tft_write_cmd(ST7789_COLMOD);
    tft_write_data(ST7789_COLMOD_16_BPP);
    Delay_Ms(10);

    tft_write_cmd(ST7789_MADCTL);
    // Restore previous orientation setting (same as initial clean implementation).
    tft_write_data((uint8_t)(ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB));
    Delay_Ms(10);

    tft_write_cmd(ST7789_INVON);
    Delay_Ms(10);

    tft_write_cmd(ST7789_DISPON);
    Delay_Ms(10);

    END_WRITE();
}

void st7789_set_cursor(uint16_t x, uint16_t y)
{
    _cursor_x = x + ST7789_X_OFFSET;
    _cursor_y = y + ST7789_Y_OFFSET;
}

void st7789_set_color(uint16_t color)
{
    _color = color;
}

void st7789_set_background_color(uint16_t color)
{
    _bg_color = color;
}

void st7789_print_char(char c, uint8_t font_scale)
{
    if (font_scale < 1) font_scale = 1;
    if (font_scale > 3) font_scale = 3;

    const uint8_t* start = &fontk[((uint8_t)c) << 3];
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

void st7789_print(const char* str, uint8_t scale)
{
    while (*str)
    {
        st7789_print_char(*str++, scale);
        _cursor_x += (FONT_WIDTH - 2) * scale;
    }
}

void st7789_print_number(int32_t num, uint16_t width)
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

    num_width = (11 - position) * (FONT_WIDTH + FONT_SPACING_HOR) - 1;
    if (width > num_width)
    {
        _cursor_x += width - num_width;
    }

    st7789_print(&str[position], 1);
}

void st7789_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    x += ST7789_X_OFFSET;
    y += ST7789_Y_OFFSET;
    START_WRITE();
    tft_set_window(x, y, x, y);
    tft_write_data16(color);
    END_WRITE();
}

void st7789_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    x += ST7789_X_OFFSET;
    y += ST7789_Y_OFFSET;

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

void st7789_draw_bitmap(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* bitmap)
{
    x += ST7789_X_OFFSET;
    y += ST7789_Y_OFFSET;
    START_WRITE();
    tft_set_window(x, y, x + width - 1, y + height - 1);
    DATA_MODE();
    spi_write_dma_repeat(bitmap, width * height << 1, 1);
    END_WRITE();
}

static void _tft_draw_fast_v_line(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    x += ST7789_X_OFFSET;
    y += ST7789_Y_OFFSET;

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
    x += ST7789_X_OFFSET;
    y += ST7789_Y_OFFSET;

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
            st7789_draw_pixel(y0, x0, color);
        }
        else
        {
            st7789_draw_pixel(x0, y0, color);
        }
        err -= dy;
        if (err < 0)
        {
            err += dx;
            y0 += step;
        }
    }
}

void st7789_draw_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    _tft_draw_fast_h_line(x, y, width, color);
    _tft_draw_fast_h_line(x, y + height - 1, width, color);
    _tft_draw_fast_v_line(x, y, height, color);
    _tft_draw_fast_v_line(x + width - 1, y, height, color);
}

void st7789_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
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

void st7789_set_scroll_area(uint16_t top_fixed, uint16_t scroll_height, uint16_t bottom_fixed)
{
    START_WRITE();
    tft_write_cmd(ST7789_VSCRDEF);
    DATA_MODE();
    spi_write((uint8_t)(top_fixed >> 8));
    spi_write((uint8_t)(top_fixed & 0xFF));
    spi_write((uint8_t)(scroll_height >> 8));
    spi_write((uint8_t)(scroll_height & 0xFF));
    spi_write((uint8_t)(bottom_fixed >> 8));
    spi_write((uint8_t)(bottom_fixed & 0xFF));
    END_WRITE();
}

void st7789_set_scroll_start(uint16_t scroll_start)
{
    START_WRITE();
    tft_write_cmd(ST7789_VSCRSADD);
    DATA_MODE();
    spi_write((uint8_t)(scroll_start >> 8));
    spi_write((uint8_t)(scroll_start & 0xFF));
    END_WRITE();
}
