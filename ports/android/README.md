# OutRunners on Android

The console version of the game (`OutRunners`) built for Android phones and tablets (Android 7 or later), as an
APK, as SegaSonic's port is. It is the same game as on a computer, with what a phone asks for:

- **The picture is WIDE, fixed**: a phone's screen is 16:9 or wider, and the game's wide picture fills its
  height: 20:9 (496 pixels) where the screen is twice as wide as high or more, else 16:9 (416, 398 shown).
  OPTIONS has no VIDEO page, and the title's menu no 2 PLAYERS.
- **Touch controls**, as few as the race needs, drawn over the picture (`ports/sdl2/touch.c`): the wheel where
  your left thumb lands on the screen's left half (it turns as far as your thumb goes across, and moves the
  menus' cursor), **GAS** (the menus' accept) and **BRAKE** on the right, **START** at the top right (start,
  and the race's pause), **UP** and **DOWN** for the gears in a race with the manual transmission alone (none
  with the automatic), and **MUSIC**, the radio, a small oval at the bottom. They hide when a game controller
  or a keyboard is used, and come back when a finger touches the screen.
- **Game controllers**, Bluetooth or USB: the left stick or the d-pad steers, the right trigger or A is the
  gas, the left trigger or B the brake, the shoulders shift up and down, Y is the radio, Start starts and
  pauses. OPTIONS has no CONTROLS page.
- **Android's back** (the button or the gesture) goes back in the menus and does nothing in a race, so a thumb
  at the screen's edge never ends a game.
- **No EXIT** on the title's menu: Android's home leaves the game, as it leaves any other. Going to the
  background pauses a race and writes what the game keeps.
- **The board's video is drawn on several cores** (`runtime/src/workers.c`): its rows on up to three of the
  phone's cores beside the game's, the left screen alone in a one player game.

What the game writes (its settings, the EEPROM, the bookkeeping, TIME ATTACK's times) stays in the app's own
folder; its log, `outrunners.log`, is in `Android/data/io.github.cheviman.outrunners/files/` on the phone's
storage.

## Making the APK (for players)

Android makes nothing from the ROM set: the APK carries the game's assets, taken out of the player's ROM set
on the player's computer, so each APK is its player's alone: **never share it**. `OutRunners APK`, a tool in
the computer's package (`apk/`), makes it: drop your `orunners.zip` on its window (and a square PNG or JPEG
for your own launcher icon), **Make OutRunners.apk**, copy it to the phone and open it there, or
`adb install -r OutRunners.apk`. It signs the APK with a key of yours, made the first time and kept in your own
folder (`%APPDATA%\OutRunners` on Windows): keep it, as only an APK with the same key installs over the last one
and keeps what the game wrote. How it works: [apk/README.md](apk/README.md).

## Building

The game's Android build, which OutRunners APK makes the player's APK of, and a computer's own APK with its
assets pack. You need:

- the Android SDK's command-line tools, with `platforms;android-35`, `build-tools` (35 or newer) and the NDK
  (28 or newer), installed with `sdkmanager`;
- a JDK 17 (`JAVA_HOME`);
- CMake and Ninja.

Then, from the repository's folder:

```bash
cmake -S . -B build/android -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=<sdk>/ndk/<version>/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DORUNNERS_REFERENCE_TOOLS=OFF
```

```bash
cmake --build build/android --target orunners_apk --parallel 6
```

`CMakeLists.txt` sees the NDK's toolchain and builds the game alone (`android.cmake`): SDL2 from its source
(2.32.10, `FETCHCONTENT_SOURCE_DIR_SDL2` picks a copy already on the computer), the game as `libmain.so`, and
two APKs around them, put together by the SDK's own tools, without Gradle:

- `build/bin/android/outrunners-base.apk`: the game without an assets pack or a signature, which OutRunners
  APK makes a player's APK of;
- `build/bin/android/OutRunners-debug.apk`: this computer's own, with its pack (`build/bin/outrunners.assets`),
  signed with the debug key of its Android tools.

| Step | Tool | What it makes |
|---|---|---|
| SDL's Java (`org.libsdl.app.SDLActivity`, from SDL's source) | `javac`, `d8` | `classes.dex` |
| The manifest, the icon, the assets (the pack stored, read in place) | `aapt2` | the APK's base |
| `libmain.so` and `libSDL2.so` (without their symbols), `classes.dex` | `llvm-strip`, `aapt` | the APK |
| | `zipalign` (and `apksigner`, this computer's) | the APKs, aligned (and signed) |

To give this computer's APK an icon of your own on the launcher, a square JPEG or PNG:
`-DORUNNERS_ANDROID_ICON=my_icon.png`, or `local/icon_android.png` in the repository's folder (`local/` holds a
player's own pictures and is never committed). The launcher shows it whole, its shape cutting the corners. By
default it has the project's wheel (`res/`, `tools/make_icon.py`: an adaptive icon's two layers).

| File | |
|---|---|
| `android.cmake` | the Android build: SDL2, `libmain.so`, the APKs |
| `AndroidManifest.xml` | the app: SDL's activity, landscape, full screen |
| `res/` | the launcher's icon (`tools/make_icon.py`), and `ic_launcher_own.xml` and `ic_launcher_art.png` for a player's own picture |
| `apk/` | OutRunners APK, the tool that makes a player's APK on their computer ([apk/README.md](apk/README.md)) |

The game's own changes for Android are in `ports/sdl2/main.c` (`__ANDROID__`: the pack in the APK, the app's
folders, back and the background), `ports/sdl2/touch.c` (`ORUNNERS_TOUCH`), `console/menu.c`
(`ORUNNERS_HANDHELD`: OPTIONS without VIDEO and CONTROLS; `ORUNNERS_SYSTEM_EXIT`: no EXIT on the title),
`console/console.c` (`console_manual_gears`: the car's transmission) and `runtime/src/tasks.c` (the game's two
tasks without ucontext, which Android's C library has not).

## Where it stands

- It runs on a phone (Sebas, 30-10-2026), its touch controls and its pack read from the APK.
- OutRunners APK's APK passes `apksigner verify` (APK Signature Scheme v2), its pack the computer's byte for
  byte.

Nothing of the game is in this repository: the APK is made on the player's computer, from the player's files.
OutRunners is a trademark of SEGA; this project is not affiliated with or endorsed by SEGA, nor with Google.
