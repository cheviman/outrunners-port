# OutRunners on iOS

The console version of the game (`OutRunners`) built for iPhone and iPad (iOS 13 or later), as an `.ipa`. It is
Android's game on Apple's devices: the picture WIDE and fixed, chosen again when the screen changes its shape; the
touch controls of `ports/sdl/touch.c`; game controllers; no EXIT on the title's menu. The home gesture pauses a
race and writes what the game keeps.

Unlike Android's APK, **the app carries nothing of the game**: it takes its assets out of your ROM set on the
device, the first time it runs, as a computer's game does. So the `.ipa` is the same for every player, and it is
never on the App Store (the game is Sega's, the player brings the ROM set).

## Installing

The release's package, `outrunners-VERSION-ios.zip` (VERSION the tag, v1.4.1; "dev" elsewhere), holds
`OutRunners.ipa` (unsigned: your sideloading tool signs it), `README.txt`, `LICENSE.txt` and `licenses/`.

Sideload it with a tool like AltStore, SideStore or Sideloadly. With a free Apple ID, the app runs for 7 days and
must be re-signed; a paid developer account lifts that. The device must have Developer Mode on (Settings > Privacy
& Security > Developer Mode).

## The ROM set

The first run looks for `orunners.zip` (MAME's set, World) in the app's folder, and otherwise says so on its
screen and in its log. To put it in the folder yourself:

- on the phone, the Files app: **On My iPhone > OutRunners** (On My iPad on an iPad);
- on a Mac, the phone in Finder: **Files > OutRunners**;
- from the command line:

```bash
xcrun devicectl device copy to --device <udid> --domain-type appDataContainer --domain-identifier io.github.cheviman.outrunners --source orunners.zip --destination Documents/orunners.zip
```

That folder also holds what the game writes (its settings, the EEPROM, the bookkeeping, TIME ATTACK's times), the
assets it took out, and its log, `outrunners.log`. Once the assets are out the ROM set can be deleted.

## Playing

The console version, WIDE to the screen's shape between the sides the notch or the Dynamic Island takes, with touch
controls (the wheel under your left thumb, GAS and BRAKE on the right, START at the top right), or a game controller
(MFi, Xbox, PlayStation, Switch Pro, through Apple's GameController framework): the touch controls hide when a
controller is used. The home gesture pauses a race and writes what the game keeps.

NETWORK races up to four devices on the same local network, computers, phones and Switches alike. iOS gives an app
no broadcasts without Apple's entitlement, so the game finds the rooms by asking each address of the local network
instead (arcade-lan's `ARCADE_LAN_NO_BROADCAST`). iOS asks for the Local Network permission the first time NETWORK
is used; refused, no room is found (Settings > Privacy & Security > Local Network turns it on).

## Building

You need a Mac with Xcode (its iOS SDK) and CMake: the build is CMake's Xcode generator. SDL 3 comes from its source
(3.4.20, static; `FETCHCONTENT_SOURCE_DIR_SDL3` picks a copy already on the computer).

For a device, `build/bin/ios/OutRunners.app` and `OutRunners.ipa` beside it, not signed:

```bash
cmake -S . -B build/ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DORUNNERS_REFERENCE_TOOLS=OFF
cmake --build build/ios --config Release --target orunners_ios_games -- CODE_SIGNING_ALLOWED=NO
```

For the Simulator, the app alone, in a build folder of its own (here `build/bin-ios-simulator/ios/OutRunners.app`):

```bash
cmake -S . -B build/ios-simulator -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DORUNNERS_REFERENCE_TOOLS=OFF -DORUNNERS_BIN_DIR=build/bin-ios-simulator
cmake --build build/ios-simulator --config Release --target orunners_ios_games -- CODE_SIGNING_ALLOWED=NO
```

`-DORUNNERS_IOS_TEAM=<team ID>` signs the app with that development team instead (a Mac building for its own
iPhone).

The app's `Info.plist` declares `UIApplicationSceneManifest`, with `UIApplicationSupportsMultipleScenes` false and
no configurations of its own: SDL 3.4 has UIKit's scene life cycle itself, which iOS 27's SDK asks every app for
(an app without it does not start), and SDL's app delegate gives the scene its delegate, `SDLUIKitSceneDelegate`.

The game's own changes for iOS are in `ports/sdl/main.c` (`SDL_PLATFORM_IOS` and `ORUNNERS_IOS`: the assets out
of the player's `orunners.zip`, the app's folders, the home gesture and the background) and `runtime/src/tasks.c`
(the game's two tasks without ucontext, as on Android).

## Not tried

It has not been run on an iPhone or iPad with SDL 3, nor in the Simulator with a ROM set. The first iOS version,
on SDL 2, was played on an iPhone on iOS 27.2.

| File | |
|---|---|
| `ios.cmake` | the iOS build: SDL 3, the `.app`, its icon, the `.ipa` |
| `Info.plist.in` | the app: landscape, full screen, its scene, its folder shown in Files |
| `LaunchScreen.storyboard` | the launch screen |
| `Assets.xcassets` | the project's icon: the car on its wheel, `tools/make_icon.py`'s |