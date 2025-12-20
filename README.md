# Macropad

I've been interested in mechanical keyboards for a while now, and recently I wanted to get a bit of experience writing firmware, so I decided to combine these two interests and create a mechanical keyboard from scratch (well, as close to "from scratch" as reasonably possible).

## Designing the PCB

The hardware design for a keyboard is fairly straightforward. A lot of credit here goes to ai03's guide located [here](https://wiki.ai03.com/books/pcb-design/), which gives pretty good direction on what a properly designed circuit should look like. Of course, I wanted to actually learn what was going on in the circuit that I was replicating, so I made sure to supplement this with reading through datasheets and general studying. I think I can confidently state what a pull-up resistor now does, at the very least.

Even though this is a 6-key keypad (which means each switch could be directly connected to it's own pin), I thought a keyboard matrix circuit would be more fun to implement. The premise here is pretty straightforward: since there aren't enough pins on microcontrollers to directly connect each switch to a key, you have to get a bit creative and arrange switches in a grid-like formation. Then, each switch will be connected to a specific grid pin and to a specific row pin. Now, say you have current flowing through a specific column, if any of the keys connected in that column are pressed, you'll be able to figure that out by simplying checking the corresponding row pins for any input. There's a few things to be considerate of however -- primarily [ghosting](https://www.dribin.org/dave/keyboard/one_html/).

## Firmware

The firmware I've written for this uses [LUFA](https://github.com/abcminiuser/lufa) to communicate via USB. I'm new to this framework, so I've built upon the Keyboard demo that's generously provided with LUFA. The relevant keypad code (e.g. what actually sets up our pins, scans through the matrix, handles debouncing, etc.) can be found in `macropad_firmware/macropad_firmware/Keyboard.c`. I do eventually want to move this out of a demo and into something built up from scratch (albeit while still using LUFA), but for now this has worked very well for a first revision.

The firmware is pretty basic: for scanning the matrix, we set each row to output LOW, and then we scan each column (configured to be input with pull-up resistor active) to detect which keys are pressed. We then store the number of scan-cycles a key has been pressed for, and once it hits some debouncing threshold (I've chosen 10, which corresponds to about a 5ms debounce period. The datasheet for the Cherry-MX Brown switches that I'm using state that the bounce time is <2 msec, so it's slightly overkill) we can then send it over in the next USB report.





