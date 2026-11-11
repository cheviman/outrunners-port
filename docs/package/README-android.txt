OutRunners for Android
======================

Sega's OutRunners (1992, System Multi 32) as a native game, decompiled to C and checked frame by frame
against MAME: github.com/cheviman/outrunners-port. This is Android's package: the game without its assets,
and the tool that makes your OutRunners.apk of it on your computer, with your own ROM set. Nothing of the
game comes in this package.

What you need
-------------
- orunners.zip, MAME's ROM set of OutRunners (World), or a folder of its files;
- a phone or tablet with Android 7 or later (arm64).

Making OutRunners.apk
---------------------
Start the tool for your computer, with a double click:

  Windows   OutRunners APK.exe
  macOS     OutRunners APK (macOS)    (the first time: right click, Open)
  Linux     OutRunners APK (Linux)

Leave it where it is, beside the folder android (the game's Android build) and font.bmp. Drop orunners.zip
on its window (or put it beside the tool), and a square PNG or JPEG of yours if you want your own launcher
icon. Make OutRunners.apk writes it beside the tool. From a command line:

  OutRunners APK --rom orunners.zip --out OutRunners.apk

Copy OutRunners.apk to the phone and open it there, or: adb install -r OutRunners.apk

The tool signs the APK with a key of yours, made the first time and kept in your user folder (%APPDATA%\
OutRunners on Windows): keep it, as only an APK with the same key installs over the last one and keeps what
the game wrote. The APK has the game's data in it: it is yours alone, never share it.

Playing
-------
The console version, WIDE to the screen's shape, with touch controls (the wheel under your left thumb, GAS
and BRAKE on the right, START at the top right), or a game controller: they hide when a controller is used.
The phone vibrates in a crash (OPTIONS > STEERING > PHONE VIBRATION turns it off). NETWORK races up to four
devices on the same local network (computers, phones and Switches alike). Android's back goes back in the
menus.

More: github.com/cheviman/outrunners-port/blob/main/ports/android/README.md

Licenses
--------
LICENSE.txt is the project's (BSD-3-Clause). licenses/ holds MAME's and ymfm's (the board's video and
sound follow their behavior and tables), arcade-lan's (BSD-3-Clause: NETWORK's rooms and datagrams), SDL2's
(zlib) and Kenney's (CC0: the letters, the controls' pictures).

OutRunners is a trademark of SEGA, and Android one of Google. This project is not affiliated with or
endorsed by SEGA or Google.
