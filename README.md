# cobalt-3
Cobalt 3 pocket computer

by Leonardo Leoni

I wanted a pocket computer I can build myself to learn through-hole soldering and get a simple basic computer. There were a lot of projects out there, ok, but I needed a kit ready to mount, without having to look for components around the world.

The core component is the Atmega328, the keyboard is serial with tactile momentary switches and the display an is a SPI OLED type with the common SH1106 driver. A toggle switch and a led complete the Cobalt 3 (giving a name was the most cool part of the job).

I put all together in the design of a simple pcb and I used an Arduino IDE to compile the O.S. in C++ language. The O.S. can be loaded through a UART serial interface. The O.S. is composed of 3 parts: one to show data on the display (data output), one to manage the keyboard (data input), and last but not least, to elaborate the Basic Language instructions (data elaboration).

Project page:
http://el9000.cc/projects
