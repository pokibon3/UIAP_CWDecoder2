# ST7735 Verification Tests

## Visual Tests
- Full-screen fill: black, white, red, green, blue
- 1px border rectangle at edges
- Crosshair through center
- Text grid with known coordinates

## Geometry Checks
- Verify top-left pixel maps to (0,0)
- Verify bottom-right pixel maps to (W-1,H-1)
- Adjust X/Y offsets if any clipping or overscan

## Color Order Checks
- RGB/BGR order test pattern
- Ensure no channel swapping (red/blue)

## Orientation Checks
- Verify 0/90/180/270 if rotation is supported
