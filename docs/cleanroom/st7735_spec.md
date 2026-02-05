# ST7735 Clean-Room Specification

## Source Documents
- ST7735 datasheet: [fill path or URL]
- CH32V003 reference: D:\src\cw_decoder3_for_uiap\docs\cleanroom\CH32V003.pdf
- Board wiring diagram: [fill path or URL]

## Target Hardware
- MCU: CH32V003
- Panel: ST7735, 80x160
- Interface: SPI 1-line TX
- Color format: 16-bit RGB565 (BGR order)
- Inversion: OFF

## Wiring (MCU -> ST7735)
- RESET: PC7
- DC: PD0
- CS: PC3 (optional if tied low)
- SCLK: PC5
- MOSI: PC6

## Geometry
- Display width: 160 px (landscape)
- Display height: 80 px (landscape)
- X offset: 0
- Y offset: 24

## Orientation
- Rotation: 0 degrees (landscape)
- MADCTL bits: MX=1, MY=0, MV=1, BGR=1
- MADCTL value: 0x68
- Rationale: [cite datasheet]

## Timing
- Reset pulse high/low: [ms]
- Sleep out delay: 120 ms
- Display on delay: 10 ms

## Command Set (minimum)
- SWRESET (0x01)
- SLPOUT (0x11)
- COLMOD (0x3A) = 0x05 (RGB565)
- MADCTL (0x36) = [bits above]
- INVOFF (0x20)
- DISPON (0x29)
- CASET (0x2A), RASET (0x2B), RAMWR (0x2C)
- VSCRDEF (0x33), VSCRSADD (0x37) [if scroll is required]
