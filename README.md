# Macropad

I've been interested in mechanical keyboards for a while now, and recently I wanted to get a bit of experience writing firmware, so I decided to combine these two interests and create a mechanical keyboard from scratch (well, as close to "from scratch" as reasonably possible).

## Designing the PCB

The hardware design for a keyboard is fairly straightforward. A lot of credit here goes to ai03's guide located [here](https://wiki.ai03.com/books/pcb-design/), which gives pretty good direction on what a properly designed circuit should look like. Of course, I wanted to actually learn what was going on in the circuit that I was replicating, so I made sure to supplement this with reading through datasheets and general studying. I think I can confidently state what a pull-up resistor now does, at the very least.

Even though this is a 6-key keypad (which means each switch could be directly connected to it's own pin), I thought a keyboard matrix circuit would be more fun to implement. The premise here is pretty straightforward: since there aren't enough pins on microcontrollers to directly connect each switch to a key, you have to get a bit creative and arrange switches in a grid-like formation. Then, each switch will be connected to a specific grid pin and to a specific row pin. Now, say you have current flowing through a specific column, if any of the keys connected in that column are pressed, you'll be able to figure that out by simplying checking the corresponding row pins for any input. There's a few things to be considerate of however -- primarily [ghosting](https://www.dribin.org/dave/keyboard/one_html/).

## Writing Firmware

tbd

