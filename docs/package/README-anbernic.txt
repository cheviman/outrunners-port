OutRunners for Anbernic's H700 handhelds
=========================================

Sega's OutRunners (1992, System Multi 32) as a native game, decompiled to C and checked frame by frame
against MAME: github.com/cheviman/outrunners-port. This is the package of Anbernic's H700 handhelds
(RG35XX, RG40XX... with their stock firmware): the game without its assets, and the tool that
makes your PORTS folder of it on your computer, with your own ROM set. Nothing of the game comes in this
package.

What you need
-------------
- orunners.zip, MAME's ROM set of OutRunners (World), or a folder of its files;
- the handheld's card, in your computer.

Making PORTS
------------
Start the tool for your computer, with a double click:

  Windows   OutRunners Anbernic.exe
  macOS     OutRunners Anbernic (macOS)    (the first time: right click, Open)
  Linux     OutRunners Anbernic (Linux)

Leave it where it is, beside the folder anbernic (the game's Anbernic build) and font.bmp. Drop
orunners.zip on its window (or put it beside the tool) and choose the card's Roms folder as the output
folder: Make PORTS puts the game in its PORTS at once, and the other ports there stay as they are. Or make
it beside the tool and copy the folder PORTS into the card's Roms. From a command line:

  OutRunners Anbernic --rom orunners.zip --out E:/Roms

The game is then in the handheld's PORTS. Its folder has the game's data in it: it is yours alone, never
share it.

Playing
-------
The console version, its 4:3 picture as large as the screen takes it: the d-pad or the left stick steers,
L1 (or R2) is the gas, L2 the brake, Y and B shift up and down, SELECT is the radio, START starts and
pauses; OPTIONS > CONTROLS gives each action another button. MENU leaves the game. What it writes goes to
its folder on the card, with outrunners.log.

More: github.com/cheviman/outrunners-port/blob/main/ports/anbernic/README.md

Licenses
--------
LICENSE.txt is the project's (BSD-3-Clause). licenses/ holds MAME's and ymfm's (the board's video and
sound follow their behavior and tables), arcade-lan's (BSD-3-Clause: NETWORK's rooms and datagrams), SDL2's
(zlib) and Kenney's (CC0: the letters, the controls' pictures).

OutRunners is a trademark of SEGA. This project is not affiliated with or endorsed by SEGA or Anbernic.
