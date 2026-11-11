OutRunners for the Nintendo Switch
==================================

Sega's OutRunners (1992, System Multi 32) as a native game, decompiled to C and checked frame by frame
against MAME: github.com/cheviman/outrunners-port. This is the Switch's package: the game without its
assets, and the tool that makes your OutRunners.nsp of it on your computer, with your own ROM set. Nothing
of the game comes in this package.

What you need
-------------
- orunners.zip, MAME's ROM set of OutRunners (World), or a folder of its files;
- prod.keys, your console's keys;
- a Switch with Atmosphere and signature patches, and an installer for .nsp files (Tinfoil, DBI,
  Goldleaf...).

Making OutRunners.nsp
---------------------
Start the tool for your computer, with a double click:

  Windows   OutRunners NSP.exe
  macOS     OutRunners NSP (macOS)    (the first time: right click, Open)
  Linux     OutRunners NSP (Linux)

Leave it where it is, beside the folder switch (the game's Switch build) and font.bmp. Its window shows
what the .nsp is made of: drop orunners.zip and prod.keys on it (or put them beside the tool; prod.keys can
also be in .switch in your user folder), and a 256 x 256 JPEG of yours if you want your own HOME menu icon.
Make OutRunners.nsp writes it beside the tool, or in the output folder you choose. From a command line:

  OutRunners NSP --keys prod.keys --rom orunners.zip --out OutRunners.nsp

Install OutRunners.nsp with your installer. It has the game's data in it: it is yours alone, never share it.

Playing
-------
The console version, its 16:9 picture filling the screen: the left stick or the d-pad steers, ZR or A is
the gas, ZL or B the brake, R and L shift up and down, Y is the radio, + starts and pauses. 2 PLAYERS with
a second controller; NETWORK races up to four devices on the same local network (computers, phones and
Switches alike). HOME leaves the game. What it writes goes to /switch/outrunners/ on the SD card.

More: github.com/cheviman/outrunners-port/blob/main/ports/switch/README.md

Licenses
--------
LICENSE.txt is the project's (BSD-3-Clause). licenses/ holds MAME's and ymfm's (the board's video and
sound follow their behavior and tables), arcade-lan's (BSD-3-Clause: NETWORK's rooms and datagrams), SDL2's
(zlib) and Kenney's (CC0: the letters, the controls' pictures).

OutRunners is a trademark of SEGA, and Nintendo Switch one of Nintendo. This project is not affiliated with
or endorsed by SEGA or Nintendo.
