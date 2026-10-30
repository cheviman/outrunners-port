# The console build: what it will change, and in what order

The port builds two games from the same decompiled program, as segasonic-port does:

- **OutRunners Arcade**: the decompiled game as it is, the same frame by frame as the reference (both
  cabinet screens side by side, coins, the test mode).
- **OutRunners**: the console version, the arcade game made for playing at home. The game's functions
  `console/replaced.txt` lists get the console's own versions (`console/*.c`, `ORUNNERS_CONSOLE`); their
  definitions in `src/` are written as `CONSOLE_ORIGINAL(name)`, which is `original_name` in the console
  build and the game's own name in the arcade one, so the arcade build runs the same program.

What changes the decompilation (src/, symbols.txt, tools/, runtime/) is done in outrunners-decomp and
merged here; this repository adds console/, the ports and their documents.

## What the game has to build on

- **Two seats, two screens.** Seat 1 is the left screen (screen block 208000, I/O chip 1, the window's
  left picture, the keyboard's player); seat 2 the right one (218000). Each runs its own frames, at 30 a
  second, swapping tasks every VBLANK; they share video RAM (seat 1 draws on NBG0 and NBG2, seat 2 on NBG1
  and NBG3) and the text layer. Both screens are 320 x 224 (video control bit 15 clear).
- **A one player game** takes the seat whose start is pressed; the other screen waits for challengers
  (game mode 7, link.c) for the entry time, then goes back to the title. **Two players** on one cabinet:
  the second start in the entry time joins the same race (race_entries_update); the first car past a fork
  takes everyone that way.
- **Sound**: each seat's requests go to their own command byte and their own copy of the MIDI handler;
  seat 1's notes are panned to the left speaker only, seat 2's to the right (pans 7 and 9, nothing on the
  other side). The two seats share the MultiPCM's 24 voices: a hidden seat that plays can take voices from
  the one shown.
- **Coins**: with free play (coin setting 26, `free_play` FF) coins are not counted and every seat's
  prompt is TURN THE IGNITION KEY with its key; the credit line says FREE PLAY. There is no PRESS START.
- **No debug mode**, used or unused. What there is: the test mode's settings (below); `flip_screen` (in
  the EEPROM, no test mode item); the staff credits code in the car presentation (shift down twice, up
  twice, left, right, left, right, brake, gas); unused, a kanji text viewer on the bitmap layer, a timing
  counter on the text layer, and the FBI and EPA notices unpacked at boot and never shown.
- **The test mode's settings**: GAME ASSIGNMENTS: difficulty 1-8 (the traffic), time difficulty -5..+5
  seconds by stage (PRE-STAGE, STAGE 1-4, FINAL STAGE; the program reads them one stage early and never
  the last), steering wheel reaction (the wheel's motor), advertise sound; COIN ASSIGNMENTS; NETWORK
  ASSIGNMENTS: stand alone, master or slave, cabinet 0-3, ISDN; volume (the six analog controls' range);
  SOUND (the 122 sounds); BOOKKEEPING; BACKUP DATA CLEAR.
- **Pictures**: the title (two scrolling tilemaps, the logo, its oval board, SEGA and (C) SEGA 1992 as
  sprites); the ranking's course panoramas across both screens; the route map's world map (two zoomable
  tilemaps); the car presentation's checked backdrop; car select's frames and turntable; the name entry's
  certificate; the WELCOME OUTRUNNERS letter. Nothing shows at boot but the cabinet's LCD (SEGA
  Enterprises).
- **The map**: 31 stages (0 the pre-stage, then levels of 2, 4, 6 and 8 stages, then the ten goals 15-1E,
  Australia to Kenya); each fork has two branches, by the side of the road (fork_next_stages). Everything
  that loads a stage reads the screen's first_stage: the demo races already start on any stage, a game
  always on 0 (the checkpoint count, the goal test, the route map and the start time assume it).
- **The link**: up to four cabinets (cabinet 0-3, master and slaves), 8 players, through the link board
  (EPR-15033); the board model has it with no other cabinet on the line (runtime/src/machine.c, MAME's
  simulation).

## The guidelines (Sebas, 29-10-2026)

1. **The build.** console/ and console/replaced.txt; the core's decompiler (arcade-decomp) writes
   CONSOLE_ORIGINAL for a game's console list beside BOARD_ORIGINAL for the board's (nothing changes where
   a game has no list); `OutRunners` and `OutRunners Arcade` (windows, .apps), a headless console build
   for scripted checks with `--actions` (the console's actions, for screenshots and checks); an icon of our
   own (tools/make_icon.py), nothing of the game's in the executables. As SegaSonic NSP and SegaSonic APK
   do, the Switch's and Android's makers take a picture of the player's own as the icon (icon.jpg beside
   them, 256 x 256): Sebas's is the arcade flyer's art, which stays out of the repository (Sega's).
2. **One screen.** The console shows seat 1's screen, the left one, and not the right one, whose task
   still runs (the program runs both seats). 2 PLAYERS shows both, side by side or one over the other (an
   option: HORIZONTAL, VERTICAL).
3. **One sound.** Both speakers get what is heard (the mix of the chips' outputs, runtime/src/sound_chips.c,
   after the hashes): in one player games the seat shown, in 2 PLAYERS both; the hidden seat kept silent at
   its source, so that it takes no voices.
4. **No coins.** Free play always, ARCADE too: no INSERT COIN and no credit line; the ignition key prompt
   stays, as the game's own start prompt.
5. **The SEGA presentation.** Before the title, a screen of its own: the title's SEGA (the sprite
   `title/title_pictures` row `sega_logo`, 64 x 16, 4 bits), large with the board's sprite zoom, its
   letters dark; cars race across it, as the title's logo cars race across the logo, and the letters
   light up as they pass, with the sound of an engine starting (sound 21, as a game starts); then, once
   its roar is over, the game's own voice of SEGA's (the test mode's sound 107: sound 58 of sound set A,
   the sound program back on its set 0 after it), and the title.
6. **The title's menu**, over the title (the logo and the palm beach), where INSERT COIN was:
   - ARCADE: the arcade game, one player, free play.
   - 2 PLAYERS: both seats, head to head (the game's own two player race), the second screen shown.
   - DELUXE, the modes of our own:
     - TIME ATTACK: a course or a route against the clock, best times kept.
     - FULL: every course, level by level: the pre-stage, then the 2 of the first level, the 4 of the
       second, the 6, the 8 and the 10 goals, 31 in all (stage 0 to 1E, the map's order), in one race:
       whichever branch the player takes at a fork leads to the next course, and the goals fork to the
       next one as well, as REVERSE's do, but the last.
     - REVERSE: the route from the hardest stages to the easiest. The goal it starts at is picked as
       SegaSonic's STAGE SELECT picks a level: on the route map (the game's own, its world map and its
       routes), each goal a marker, and the route shown from the top down, from the goal to the
       pre-stage; each fork leads back a level (first_stage for the start). The courses run as they
       are, not mirrored.
   - OPTIONS (Sebas):
     - CONTROLS: the buttons: each action's keys and game controller buttons, with Kenney's icons, kept
       in a file.
     - DIFFICULTY: the test mode's 1-8 (the traffic).
     - TIME: the test mode's seconds by stage (-5..+5: PRE-STAGE, STAGE 1-4, FINAL STAGE).
     - SOUND TEST: the game's sounds and songs.
     - VIDEO: WINDOW, FULL SCREEN, PIXELS, WIDE, the 2 PLAYERS layout.
     - VOLUME, as SegaSonic's.
     - BACK.
   - EXIT.
   OPTIONS over the title's opening: the palm beach in the dusk, dark, with the cars going by. DELUXE's
   pages over the ranking's course panoramas or the route map's world map.
7. **Video.** PIXELS as in SegaSonic: sharp, smooth (sharp bilinear), plain bilinear, and CRT (scanlines,
   a phosphor mask, a little vignette). WIDE: a 16:9 picture as SegaSonic's: the board's 416 pixel mode,
   the game drawing 48 more pixels at each side (the road, the tilemaps, the sprites the game leaves out
   past the edges).
8. **Kenney** (CC0): the letters of the window's own messages (Kenney Mini, as SegaSonic's
   ports/sdl2/font) and the controls' icons (Input Prompts Pixel, ports/sdl2/prompts) in CONTROLS.
9. **The ports**: the same as SegaSonic's: Windows, Linux and macOS from the CI with a package each; the
   Nintendo Switch (an .nsp made on the player's computer by OutRunners NSP), Android (an APK made by
   OutRunners APK, touch controls) and Anbernic's H700 handhelds. Of the devices, only the Switch has
   2 PLAYERS, side by side only; WIDE on the Switch and Android, not on Anbernic's handhelds (4:3).
   Android's touch controls, as few as the race needs (Sebas): the wheel, gas and brake; the gear
   buttons only with the manual transmission chosen in the car select (none with the automatic); the
   music (the radio) a small oval button at the bottom.
10. **Network**: up to 8 players, each on a device of their own: the game's own link of four cabinets.

## Done (29-10-2026)

- The build: CONSOLE_ORIGINAL in the core (arcade-decomp, its console-original branch), console/, `OutRunners`
  and `OutRunners Arcade`, orunners_console_headless with `--actions` (tests/console/). The arcade build is
  the same as the reference in the scenarios.
- One screen (the left seat's; both in 2 PLAYERS), one sound (sound_chips_join_sides; the right seat's
  requests dropped while it is not in a game), free play (load_settings), no credits line, no coin or
  operator's switches.
- The SEGA presentation (presentation.c): the title's SEGA four times its size on black, dark under a
  shadow that the three cars racing across it take away, the engine (sound 21), the fades; START skips it.
  The boot goes straight to it: no NETWORK CHECK on the screens (network_check_stand_alone).
- PRESS START in the attract mode (the prompt's message and ignition key end at once); the title's menu
  under the logo; OPTIONS over the title's opening, darkened: CONTROLS (two columns, Kenney's Input
  Prompts Pixel drawn by the window, controls.ini), DIFFICULTY by name, TIME by stage, VIDEO, VOLUME, SOUND
  TEST. DIFFICULTY and TIME reach the races at once (console_settings_to_race) and the EEPROM.
- 2 PLAYERS: both seats in one race, the window's two screens side by side or one over the other.
- PIXELS: sharp, smooth, bilinear, CRT. WIDE.
- & KNUCKLES (DELUXE's name, Sebas): TIME ATTACK with the fastest run's ghost, see-through, while it is on
  the player's route (ghost.c); REVERSE from the goal picked, its start line in the pre-stage's colours
  (the video's extra palette) and its finish where the pre-stage's fork was; FULL, the 31 courses in one
  race, its total time at the goal (routes.c); REVERSE's goal picked on the route map of the race's end,
  whole, its ten goals, a car driving to the one picked (route_select.c); CLASSIC, the arcade's game as it
  is (WAITING FOR CHALLENGERS, the right seat's START joins, its screen shown).
- The menu's song (Magical Sound Shower) and sounds (the name entry's letters); one player with no
  WAITING FOR CHALLENGERS (waiting_banner_show); WIDE on every screen; the pause in a race (Escape, Enter,
  a game controller's Start): the frame in grey, PAUSE, CONTINUE and QUIT GAME in the game's letters (the
  video's overlay of sprites).

## Done (30-10-2026)

- The devices' menus (console/menu.c): no VIDEO on a game device, its picture fixed (WIDE on the Switch,
  4:3 on Anbernic's handhelds, smooth pixels); CONTROLS on Anbernic's handhelds only; 2 PLAYERS on the
  Switch only, side by side; no EXIT where the system leaves games (the Switch).
- The Switch (ports/switch): the game as an .nro with the pack in its romfs and as the .nsp's pieces,
  devkitPro's toolchain; the game's two tasks on stacks of their own without ucontext (runtime/src/tasks.c,
  aarch64: the Switch and Android); OutRunners NSP, SegaSonic NSP's with the pack made from the ROM set
  (ports/switch/nsp/rom_pack.c); tools/check_nsp.py. Runs on Eden (an emulator), the .nro and the .nsp.
- The video draws the screens the window shows alone (s32_video_show_screens): the left one in a one player
  game, its layers, sprites and mixing; the headless runs draw both.
- OutRunners Anbernic (ports/anbernic/maker), as OutRunners NSP (Sebas): PORTS for the handheld's card from
  the game's Anbernic build (build/bin/anbernic, no assets in it) and the player's ROM set, written beside
  the tool or straight into the card's Roms.
- Android (ports/android): libmain.so and the APKs with the NDK and the SDK's tools, without Gradle; the touch
  controls (ports/sdl2/touch.c: the wheel, GAS, BRAKE, START, the gears with the manual transmission alone,
  MUSIC a small oval at the bottom); OutRunners APK, SegaSonic APK's with the pack made from the ROM set.
- The project's icon (tools/make_icon.py): a steering wheel on a sunset, for the builds without a picture of
  the player's own. Sebas's own (local/, never committed): the car for the computer, the flyer for the
  Switch, the title's oval for Android.
- REVERSE's map: no pause over it (it is the race's step 5), so START confirms the goal.
- WIDE 16:9 and 20:9 (Sebas): OPTIONS > VIDEO's WIDE OFF, 16:9 (48 columns more at each side, 416) or 20:9
  (88, 496); Android takes 20:9 where its screen is twice as wide as high or more, the Switch 16:9. 16:9's
  frames the same as before (but VIDEO's line); REVERSE's map kept within its right edge at 20:9.
- REVERSE's time turned round (Sebas: its first course, a goal's, the longest, ran out at 40 seconds): the
  start 99 seconds, the clock's most beside FINAL STAGE (the setting the car adds to its 40, for that one
  start), and the 59 more taken back from the last checkpoints (the pre-stage's gives nothing, the first
  level's 13): the race's whole time the game's own, its margin at the start.

- The computers' executables' icon: the project's wheel (ports/sdl2/icon: a resource on Windows, which SDL
  gives the window, the .apps' on macOS, a picture beside them on Linux); on Windows a picture of the
  player's own where the computer has one (local/icon_pc.png or ORUNNERS_PC_ICON, made an .ico as CMake
  configures): Sebas's, the car.
- The CI (.github/workflows/build.yml, as SegaSonic's): the Switch's .nsp pieces, Android's base APK and
  Anbernic's game built each in its own job, in every computer's package beside OutRunners NSP, OutRunners APK
  and OutRunners Anbernic, with docs/package/README.txt; a tag v* makes a release of the packages.

## To do

- The decompilation's new version merged (Sebas: before the network).
- The network, last (Sebas).
- 2 PLAYERS on the Switch up to 4 players a console (Sebas, 30-10-2026), the screen in four, with the
  network's multiplayer.

## Order

1. The build: CONSOLE_ORIGINAL in the core, console/, the two executables, the headless console build.
2. One screen, one sound, no coins.
3. The SEGA presentation and the title's menu (ARCADE, 2 PLAYERS, OPTIONS, EXIT; DELUXE's place).
4. OPTIONS: the test mode's settings, VIDEO with PIXELS, CONTROLS with Kenney's icons, VOLUME, SOUND TEST.
5. 2 PLAYERS with its two layouts.
6. DELUXE: TIME ATTACK, FULL, REVERSE.
7. WIDE.
8. The first run without Python: the game takes its assets out of the player's orunners.zip itself (a C
   extractor that writes the folder extract_assets.py writes), as SegaSonic's does; then the CI's packages,
   the Switch, Android and Anbernic.
9. The network.
