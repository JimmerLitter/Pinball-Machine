# Pinball Machine — ECE 115

A laser-cut plywood pinball machine with an Arduino-driven scoring system, solenoid flippers, a spring plunger, and a two-digit seven-segment score display.

## The machine

The playfield: two flippers, a spinner target, the seven-segment score display on the backpanel, and the spring-loaded plunger along the right rail.

![Playfield](Machine%20Images/playfield.jpeg)

Underneath the backpanel: the Arduino Mega, an L298N motor driver, the DC power supply, and the breadboards carrying the switch, piezo, and IR beam-break wiring.

![Wiring](Machine%20Images/wiring.jpeg)

## Repository layout

| Path | Contents |
| --- | --- |
| `arduino code/main/` | The full game sketch — scoring, sound, motor, and IR beam-break logic |
| `arduino code/limitswitch/` | Standalone test for the scoring limit switch |
| `arduino code/ball_handler_servo/` | Standalone sweep test for the ball-handler servo |
| `7segment/` | Standalone driver for the shift-register seven-segment display |
| `Part files/` | SolidWorks parts, STLs for printing, and DXF/SVG cut files |

## Scoring inputs

`arduino code/main/main.ino` counts a point from either of two sources: a limit switch on pin 2, and a piezo sensor on `A1` that fires above a tuned threshold with a 250 ms lockout so a single hit registers once. Scores are shifted out to the display through the `data`/`latch`/`CLK` pins.

## Fabrication

Parts in `Part files/` are stored with Git LFS (see `.gitattributes` for the `.SLDPRT`, `.STL`, and `.DXF` filters). Clone with `git lfs install` first, or the part files will arrive as text pointers instead of geometry. The `.DXF` and `.svg` files are the flat patterns for the laser-cut panels; the `.stl` files are the printed brackets, mounts, and housings.
