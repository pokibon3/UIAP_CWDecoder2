#ifndef ST7789_DRIVER_H
#define ST7789_DRIVER_H

#include <stdint.h>

// Panel resolution and RAM offsets
#define ST7789_WIDTH    240
#define ST7789_HEIGHT   135
#define ST7789_X_OFFSET 40
#define ST7789_Y_OFFSET 53

// Optional: pull CS low and define ST7789_NO_CS
// #define ST7789_NO_CS

#define RGB565(r, g, b) ((((r)&0xF8) << 8) | (((g)&0xFC) << 3) | ((b) >> 3))
#define BGR565(r, g, b) ((((b)&0xF8) << 8) | (((g)&0xFC) << 3) | ((r) >> 3))
#define RGB             RGB565

#define BLACK       RGB(0, 0, 0)
#define NAVY        RGB(0, 0, 123)
#define DARKGREEN   RGB(0, 125, 0)
#define DARKCYAN    RGB(0, 125, 123)
#define MAROON      RGB(123, 0, 0)
#define PURPLE      RGB(123, 0, 123)
#define OLIVE       RGB(123, 125, 0)
#define LIGHTGREY   RGB(198, 195, 198)
#define DARKGREY    RGB(123, 125, 123)
#define BLUE        RGB(0, 0, 255)
#define SKYBLUE     RGB(0, 204, 255)
#define DARKBLUE    RGB(0, 0, 192)
#define GREEN       RGB(0, 255, 0)
#define CYAN        RGB(0, 255, 255)
#define RED         RGB(255, 0, 0)
#define MAGENTA     RGB(255, 0, 255)
#define YELLOW      RGB(255, 255, 0)
#define WHITE       RGB(255, 255, 255)
#define ORANGE      RGB(255, 165, 0)
#define GREENYELLOW RGB(173, 255, 41)
#define PINK        RGB(255, 130, 198)

#define FONT_SCALE_8X8   1
#define FONT_SCALE_16X16 2

// Initialize display controller and SPI/DMA
void st7789_init(void);
// Set cursor position for text rendering
void st7789_set_cursor(uint16_t x, uint16_t y);
// Set text foreground color
void st7789_set_color(uint16_t color);
// Set text background color
void st7789_set_background_color(uint16_t color);
// Draw a single character at the current cursor
void st7789_print_char(char c, uint8_t scale);
// Draw a string at the current cursor
void st7789_print(const char* str, uint8_t scale);
// Draw an integer with optional field width
void st7789_print_number(int32_t num, uint16_t width);
// Draw a single pixel
void st7789_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
// Draw a line segment
void st7789_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
// Draw a rectangle outline
void st7789_draw_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
// Fill a rectangle
void st7789_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
// Draw a 16bpp bitmap
void st7789_draw_bitmap(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* bitmap);
// Set vertical scroll area
void st7789_set_scroll_area(uint16_t top_fixed, uint16_t scroll_height, uint16_t bottom_fixed);
// Set vertical scroll start offset
void st7789_set_scroll_start(uint16_t scroll_start);

#endif
