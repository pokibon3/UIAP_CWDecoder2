# ST7789 Clean-Room Specification

## Source Documents
- ST7789 datasheet: D:\src\cw_decoder3_for_uiap\docs\cleanroom\ST7789.pdf
- CH32V003 reference: D:\src\cw_decoder3_for_uiap\docs\cleanroom\CH32V003.pdf
- Board wiring diagram: [fill path or URL]

## Target Hardware
- MCU: CH32V003
- Panel: ST7789, 1.14" 135x240
- Interface: SPI 1-line TX
- Color format: 16-bit RGB565
- Inversion: ON

## Wiring (MCU -> ST7789)
- RESET: PC7
- DC: PD0
- CS: PC3 (optional if tied low)
- SCLK: PC5
- MOSI: PC6

## Geometry
- Display width: 240 px (landscape)
- Display height: 135 px (landscape)
- X offset: 40
- Y offset: 53

## Orientation
- Rotation: 0 degrees
- MADCTL bits: MX=1, MY=0, MV=1, RGB=1, BGR=0
- MADCTL value: 0x60
- Rationale: [cite datasheet]

## Timing
- Reset pulse high/low: [ms]
- Sleep out delay: 120 ms
- Display on delay: 10 ms

## Command Set (minimum)
- SLPOUT (0x11)
- COLMOD (0x3A) = 0x55 (RGB565)
- MADCTL (0x36) = [bits above]
- INVON (0x21)
- DISPON (0x29)
- CASET (0x2A), RASET (0x2B), RAMWR (0x2C)
- VSCRDEF (0x33), VSCRSADD (0x37) [if scroll is required]
