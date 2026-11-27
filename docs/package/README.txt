OutRunners
==========

Sega's OutRunners (1992, System Multi 32) as a native game, decompiled to C and checked frame by frame
against MAME: github.com/cheviman/outrunners-port. Nothing of the game comes in this package: it takes
the game's assets out of your own ROM set.

What you need
-------------
orunners.zip, MAME's ROM set of OutRunners (World). Keep it beside the game, or drop it on the game's
window when it asks for it.

The games
---------
OutRunners           the console version: one screen, a menu (ARCADE, 2 PLAYERS, NETWORK, & KNUCKLES with
                     CLASSIC, TIME ATTACK, FULL and REVERSE, OPTIONS), WIDE up to 21:9, the pause, game
                     controllers and keys set in OPTIONS > CONTROLS
OutRunners Arcade    the arcade game as it is, both cabinet screens side by side

The first time either starts it takes the assets out of orunners.zip (a few seconds) and keeps them:
beside the game on Windows, in your user folder for the game elsewhere. Started with --game and the ROM
set's path (OutRunners --game D:\roms\orunners.zip), it takes them from there, as a shortcut or a launcher
can. recomplauncher.json tells RecompLauncher (recomplauncher.com) how to do it all for you: it asks for
orunners.zip once, and shows the video settings on the game's page.

What the game writes (the records, outrunners.ini, controls.ini, TIME ATTACK's ghosts) and its log,
outrunners.log with the two runs before beside it, go to the same folder: beside the game on Windows, and on
Linux and macOS too where a file portable.txt is beside the game (beside the .app); else ~/.local/share/
OutRunners on Linux and ~/Library/Application Support/OutRunners on macOS. A game started from a terminal
says it there instead.

Racing wheels
-------------
A wheel the game takes as a game controller (a Logitech G29, for one) works as it is: the wheel is the
left stick and the pedals the triggers; set OPTIONS > STEERING > WHEEL ROTATION to its own turn (900 on a
G29) and the DEAD ZONE to 0%. A wheel the game does not see (a Thrustmaster T300RS on Windows): OPTIONS >
STEERING > SET UP WHEEL asks for its wheel, pedals and buttons one by one, and keeps them in
gamecontrollerdb.txt in the same folder, where SDL's controller layouts made elsewhere can go too.
Pedals and a shifter plugged in on their own are taken too, as they are pressed: they are kept in
wheel_devices.txt, and the wheel is player 1's, its force feedback with it.
STEERING's INPUT TEST shows the wheel and the pedals as the game sees them.

NETWORK
-------
Up to four devices on the same local network (a house's Wi-Fi, a phone's hotspot) race together:
computers, phones and the Switch alike. NETWORK > NAME is your name; one device picks HOST and waits for
the others, which pick JOIN and the room; the host's START races them all. A device that stops being
heard is out of the race after 3 seconds, and the race goes on. The game uses UDP (the rooms on port
27720): a firewall that asks about the game should let it reach the private network.

The devices
-----------
The Nintendo Switch, Android, Anbernic's H700 handhelds and the R36S on ArkOS have packages of their own on
the same release page: outrunners-VERSION-switch.zip, outrunners-VERSION-android.zip,
outrunners-VERSION-anbernic.zip and outrunners-VERSION-r36s.zip (VERSION is the release's, v1.4.2 say). Each has the device's game without assets and the tool, for
Windows, macOS and Linux, that makes yours on your computer with your ROM set.

Licenses
--------
LICENSE.txt is the project's (BSD-3-Clause). licenses/ holds MAME's and ymfm's (the board's video and
sound follow their behavior and tables), arcade-lan's (BSD-3-Clause: NETWORK's rooms and datagrams,
github.com/cheviman/arcade-lan), SDL's (zlib) and Kenney's (CC0: the windows' letters, the controls'
pictures).

OutRunners is a trademark of SEGA. This project is not affiliated with or endorsed by SEGA, Nintendo,
Google or Anbernic.
