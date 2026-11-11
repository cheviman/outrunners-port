# OutRunners on iOS

The console version of the game (`OutRunners`) built for iPhone and iPad (iOS 27.2 or later), and tested on an
iPhone Duo, folded screen and all. It is Android's game (`ports/android/README.md`): the picture WIDE and fixed, chosen
again when the screen changes its shape; the touch controls of `ports/sdl2/touch.c`; game controllers; no EXIT on the
title's menu. Going to the background pauses a race and writes what the game keeps.

Unlike Android's APK, **the app carries nothing of the game**: it takes its assets out of your ROM set on the phone, the
first time it runs, as a computer's game does. So the .ipa is the same for every player.

## Gyro steering

**GYRO**, beside START, turns the wheel with the phone: hold it as you like and tap GYRO, which lights up, and turn it
like a steering wheel, all the way at 30 degrees. It is measured from how the phone was held at that tap, so tap it off
and on to centre it again. A thumb on the touch wheel goes over it for as long as it stays there, the menus are moved
with the thumb alone, and it starts off each time the game does. It is the first player's, and 2 PLAYERS with the
screens facing has none.

## The ROM set

The first run looks for `orunners.zip` (MAME's set, World) in the app's folder, and otherwise asks for it: tap the
screen and choose it in the system's file picker. To put it in the folder yourself:

- on the phone, the Files app: **On My iPhone > OutRunners**;
- on a Mac, the phone in Finder: **Files > OutRunners**;
- from the command line:

```bash
xcrun devicectl device copy to --device <udid> --domain-type appDataContainer --domain-identifier io.github.yoshisuga.outrunners --source orunners.zip --destination Documents/orunners.zip
```

That folder also holds what the game writes (its settings, the EEPROM, the bookkeeping, TIME ATTACK's times), the
assets it took out, and its log, `outrunners.log`. Once the assets are out the ROM set can be deleted.

## Building

You need Xcode (its iOS 27 SDK), CMake and Ninja. SDL2 comes from its source (2.32.10,
`FETCHCONTENT_SOURCE_DIR_SDL2` picks a copy already on the computer).

For the simulator, `build/ios-sim/bin/OutRunners.app`:

```bash
cmake -S . -B build/ios-sim -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=27.2 -DORUNNERS_REFERENCE_TOOLS=OFF
```

```bash
cmake --build build/ios-sim --target OutRunners --parallel 8
```

For a device, `build/ios-device/bin/OutRunners.ipa`, not signed:

```bash
cmake -S . -B build/ios-device -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=27.2 -DORUNNERS_REFERENCE_TOOLS=OFF
```

```bash
cmake --build build/ios-device --target outrunners_ipa --parallel 8
```

Sign it with your own development profile (embed it as `embedded.mobileprovision` and `codesign` the .app with its
entitlements; any re-signing tool does this) and install it with `xcrun devicectl device install app`.
`-DORUNNERS_IOS_BUNDLE_ID=...` sets the bundle identifier, which an explicit profile needs to match.

| File | |
|---|---|
| `ios.cmake` | the iOS build: SDL2, the .app, its icon, the .ipa |
| `Info.plist.in` | the app: landscape, full screen, its scene, its folder shown in Files |
| `ios.m`, `ios.h` | the scene, the app's folder, the file picker |

The game's own changes for iOS are in `ports/sdl2/main.c` (`ORUNNERS_IOS`, `PHONE`, `FIRST_RUN`),
`ports/sdl2/first_run.c` (the tap instead of a drop) and `runtime/src/tasks.c` (the game's two tasks without ucontext,
as on Android).

## SDL 2 on the iOS 27 SDK

Apps built with this SDK must use the UIScene life cycle, or they do not start, and SDL 2 predates it. `ios.m` adopts it
around SDL, which stays unpatched: the app's scene delegate waits for the scene, and SDL's window goes in it when it is
made. `-[UIApplication statusBarOrientation]` does nothing on iOS 27, so SDL takes the display to be upright (the log's
`screen 466x678`); the renderer's size, which the game uses, is the true one.

## NETWORK and broadcasts

iOS only sends and receives UDP broadcasts with Apple's multicast entitlement
(`com.apple.developer.networking.multicast`), which Apple grants on request and a plain development profile does not
have. The rooms are [arcade-lan](https://github.com/cheviman/arcade-lan)'s (`lan/`), which finds them by their
broadcast beacons alone: an app signed with the entitlement hosts and joins as any other device, and one without it
finds no room and its own is not found (iOS asks for the Local Network permission the first time NETWORK is used;
refused, the same happens, and Settings > Privacy & Security > Local Network turns it on).

This port's first version found the rooms without the entitlement, by unicast, in the sockets the game had then
(`console/net_udp.c`): a room hosted on iOS sent its beacon once a second to every other address of the local
network, a /24 at most, and a device searching on iOS sent a `PACKET_SEEK` the same way to port 27721, where every
room's host listened and answered with its beacon. The game's network has since moved to arcade-lan, which does not
have that search yet: it is what arcade-lan needs for an iPhone without the entitlement to play NETWORK.

## Where it stands

- In the iPhone Duo's simulator: the first run from a ROM set in the app's folder, 60 frames a second, a race driven
  with the touch controls, the pause and the saves when it goes to the background.
- On an iPhone Duo: the same first run, and 60 frames a second by its log. The controls on the phone itself have
  not been tried.
- The file picker opens, but choosing a set in it has not been tried.
- NETWORK's discovery was tried in the simulator with this port's first sockets; over arcade-lan it has not been
  tried. Two real devices have not raced yet.
- Folding and unfolding mid-game has not been tried on the phone.
- Gyro steering is new: its direction and strength are worth a check on your phone.

Nothing of the game is in this repository. OutRunners is a trademark of SEGA; this project is not affiliated with or
endorsed by SEGA, nor with Apple.
