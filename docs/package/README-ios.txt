OutRunners for iPhone and iPad
==============================

Sega's OutRunners (1992, System Multi 32) as a native game, decompiled to C and checked frame by frame
against MAME: github.com/cheviman/outrunners-port. This is the package
for iOS and iPadOS (13 or later, an iPhone or an iPad of the last years: arm64): the game as an .ipa
file, without its assets and not signed. Nothing of the game comes in this package: the game takes its
graphics and sounds out of your own ROM set.

It is not on the App Store, and never will be: the game is Sega's, and you bring the ROM set. You
install it yourself ("sideloading"), and the tool you install it with signs it for your device.

What you need
-------------
- orunners.zip, MAME's ROM set of OutRunners (World), as MAME has it;
- an iPhone or an iPad, and a computer (Windows or macOS) for the tool that installs the game.

Installing
----------
The .ipa file is in the package: OutRunners.ipa. Install it with one of these, which sign an app with
your own Apple ID as they install it:

- AltStore (altstore.io) or SideStore (sidestore.io): install it on the device as its site says, then
  open the .ipa with it (AltStore's My Apps, the + at the top).
- Sideloadly (sideloadly.io), on a computer: the device on USB, drop the .ipa on its window, your Apple
  ID, Start.

With a free Apple ID an app installed so stops opening after 7 days: open AltStore or SideStore to
refresh it (they do it themselves in the background with their companion on the computer), or install it
again with Sideloadly; a refresh keeps the game's folder as it was. A paid developer account signs for a
year. Where the iOS version allows it, TrollStore installs an .ipa for good.

The first time on iOS 16 or later, the device asks for Developer Mode (Settings, Privacy & Security,
Developer Mode) and to trust your Apple ID (Settings, General, VPN & Device Management).

The ROM set
-----------
The game has its own folder, which the Files app shows: open the game once (it says it has no ROM set),
then in the Files app go to On My iPhone (On My iPad), OutRunners, and put orunners.zip there. From a
computer: Finder (or the Apple Devices app on Windows), the device, Files, the game, and drop
orunners.zip on it. Open the game again: the first start takes the assets out of orunners.zip into the
folder there (some seconds); from then on the game starts from them, and orunners.zip can go.

What the game makes from your ROM set is yours alone: never share it.

Playing
-------
The console version, WIDE to the screen's shape (between the sides the notch or the Dynamic Island takes),
with touch controls (the wheel under your left thumb, GAS and BRAKE on the right, START at the top right),
or a game controller (MFi, Xbox, PlayStation, Switch Pro): they hide when a controller is used. NETWORK
races up to four devices on the same local network (computers, phones and Switches alike): the first time,
iOS asks whether the game may use the local network. Going to the home screen pauses a race; swipe up from
the bottom (twice: the first swipe only shows the bar) to leave the game. The game keeps its settings, its
records, TIME ATTACK's ghosts and its log (outrunners.log) in its folder, beside the assets.

More: github.com/cheviman/outrunners-port/blob/main/ports/ios/README.md

Licenses
--------
LICENSE.txt is the project's (BSD-3-Clause). licenses/ holds MAME's and ymfm's (the board's video and
sound follow their behavior and tables), arcade-lan's (BSD-3-Clause: NETWORK's rooms and datagrams),
SDL's (zlib) and Kenney's (CC0: the letters, the controls' pictures).

OutRunners is a trademark of SEGA, and iPhone, iPad and iOS are trademarks of Apple. This project is not
affiliated with or endorsed by SEGA or Apple.