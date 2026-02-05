# Clean-Room ST7789 Driver Plan

This folder defines the clean-room process and captures only specification-level
information. Do not place or copy any third-party source code here.

## Rules
- Do not read or reference existing ST7789 driver source code.
- Only use primary documents (datasheet, MCU reference, wiring).
- Record every assumption with a citation to a primary document.
- Keep implementation decisions separate from the spec.

## Files
- st7789_spec.md: Panel/MCU wiring, geometry, rotation, offsets, color order.
- st7789_init.csv: Initialization command list (sequence + delays).
- st7789_tests.md: Test plan for verification on hardware.
