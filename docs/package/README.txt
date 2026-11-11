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
                     CLASSIC, TIME ATTACK, FULL and REVERSE, OPTIONS), WIDE 16:9 and 20:9, the pause, game
                     controllers and keys set in OPTIONS > CONTROLS
OutRunners Arcade    the arcade game as it is, both cabinet screens side by side

The first time either starts it takes the assets out of orunners.zip (a few seconds) and keeps them:
beside the game on Windows, in your user folder for the game elsewhere.

Racing wheels
-------------
A wheel the game takes as a game controller (a Logitech G29, for one) works as it is: the wheel is the
left stick and the pedals the triggers; set OPTIONS > STEERING > WHEEL ROTATION to its own turn (900 on a
G29) and the DEAD ZONE to 0%. A wheel the game does not see (a Thrustmaster T300RS on Windows): OPTIONS >
STEERING > SET UP WHEEL asks for its wheel, pedals and buttons one by one, and keeps them in
gamecontrollerdb.txt in the same folder, where SDL's controller layouts made elsewhere can go too.
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
The Nintendo Switch, Android and Anbernic's H700 handhelds have packages of their own on the same release
page: outrunners-switch.zip, outrunners-android.zip and outrunners-anbernic.zip. Each has the device's game
without assets and the tool, for Windows, macOS and Linux, that makes yours on your computer with your ROM
set.

Licenses
--------
LICENSE.txt is the project's (BSD-3-Clause). licenses/ holds MAME's and ymfm's (the board's video and
sound follow their behavior and tables), arcade-lan's (BSD-3-Clause: NETWORK's rooms and datagrams,
github.com/cheviman/arcade-lan), SDL2's (zlib) and Kenney's (CC0: the windows' letters, the controls'
pictures).

OutRunners is a trademark of SEGA. This project is not affiliated with or endorsed by SEGA, Nintendo,
Google or Anbernic.
