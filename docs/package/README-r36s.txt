OutRunners for the R36S on ArkOS
================================

Sega's OutRunners (1992, System Multi 32) as a native game, decompiled to C and checked frame by frame
against MAME: github.com/cheviman/outrunners-port. This is the package of the R36S and its RK3326 kin
(R36XX...) running ArkOS, with PortMaster: the game without its assets, and the tool that makes your ports
folder of it on your computer, with your own ROM set. Nothing of the game comes in this package.

What you need
-------------
- orunners.zip, MAME's ROM set of OutRunners (World), or a folder of its files;
- the handheld's card in your computer: its EASYROMS partition;
- PortMaster on the handheld (ArkOS has it).

Making ports
------------
Start the tool for your computer, with a double click:

  Windows   OutRunners R36S.exe
  macOS     OutRunners R36S (macOS)    (the first time: right click, Open)
  Linux     OutRunners R36S (Linux)

Leave it where it is, beside the folder r36s (the game's R36S build) and font.bmp. Drop orunners.zip on its
window (or put it beside the tool) and choose the card's EASYROMS as the output folder: Make ports puts the
game in its ports at once, and the other ports there stay as they are. Or make it beside the tool and copy
the folder ports into the card's EASYROMS. From a command line:

  OutRunners R36S --rom orunners.zip --out E:/

The game is then in the handheld's Ports. Its folder has the game's data in it: it is yours alone, never
share it.

Playing
-------
The console version, its 4:3 picture as large as the screen takes it: the d-pad or the left stick steers,
L1 (or R2) is the gas, L2 the brake, Y and B shift up and down, SELECT is the radio, START starts and
pauses; OPTIONS > CONTROLS gives each action another button. SELECT and START together leave the game
(PortMaster's buttons). The RK3326 does not draw every frame in time: one that comes late is not drawn, so
that the game and its sound keep their speed. What it writes goes to its folder on the card, with
outrunners.log.

More: github.com/cheviman/outrunners-port/blob/main/ports/r36s/README.md

Licenses
--------
LICENSE.txt is the project's (BSD-3-Clause). licenses/ holds MAME's and ymfm's (the board's video and
sound follow their behavior and tables), arcade-lan's (BSD-3-Clause: NETWORK's rooms and datagrams), SDL's
(zlib) and Kenney's (CC0: the letters, the controls' pictures).

OutRunners is a trademark of SEGA. This project is not affiliated with or endorsed by SEGA.
