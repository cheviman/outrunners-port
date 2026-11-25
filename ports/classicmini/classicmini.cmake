# Name:        classicmini.cmake
# Description: the build for the Nintendo NES and SNES Classic (ARMv7 Cortex-A7, Mali-400 on fbdev, kernel 3.4): the
#              console game in its window, with the console's own SDL2
# Version:     0.1.0
# Created:     21-11-2026
# Author:      DefKorns
#
# license:BSD-3-Clause
#
# Included by CMakeLists.txt with ORUNNERS_CLASSICMINI and the toolchain gcc13_armv7.cmake. The window port
# is the generic one (ports/sdl2: OPTIONS, the assets made from the ROM set on the first run), with what the
# console asks for: SDL 2.0.4, the one the firmware ships, so sdl2/sdl_compat.h stands in for what its API
# lacks (rumble, the controllers' type, vsync, the textures' scale mode, ...) and sdl2/SDL2Config.cmake gives
# the headers and a library to link against, both from the toolchain image (build.sh); a frame past its time
# is not drawn (ORUNNERS_PACING, the R36S's ORUNNERS_FRAMESKIP), as the console's four Cortex-A7 cores take
# about 20 ms to draw one. launcher: ports/classicmini/clvg, a game for Hakchi.

set(SDL2_DIR ${CMAKE_SOURCE_DIR}/ports/classicmini/sdl2 CACHE PATH "" FORCE)
set(ORUNNERS_CLASSICMINI_SDL_DIR ${CMAKE_SOURCE_DIR}/build/classicmini-sdl CACHE PATH
	"SDL 2.0.4's headers and its libSDL2.so, from the toolchain image (ports/classicmini/build.sh)")
include_directories(${ORUNNERS_CLASSICMINI_SDL_DIR}/include/SDL2)
add_compile_definitions(ORUNNERS_PACING ORUNNERS_FRAMESKIP=2)
add_compile_options($<$<COMPILE_LANGUAGE:C>:-include${CMAKE_SOURCE_DIR}/ports/classicmini/sdl2/sdl_compat.h>)
