# Name:        anbernic.cmake
# Description: the build for Anbernic's H700 handhelds (the RG40XX H...) on their stock firmware: the console
#              game alone, as Roms/PORTS' OutRunners.sh and its folder outrunners/
# Version:     0.1.0
# Created:     30-10-2026
# Modified:    30-10-2026
# Author:      Sebastian Toyos
#
# Included by CMakeLists.txt with ORUNNERS_ANBERNIC (and the toolchain zig_aarch64.cmake: aarch64, glibc 2.35),
# as SegaSonic's port does. The game is the console build with its picture fixed (ORUNNERS_HANDHELD: the
# whole screen, the game's 4:3 picture, the video on the four cores; no VIDEO, no 2 PLAYERS), CONTROLS for its
# own buttons (ORUNNERS_CONTROLS_PAGE), the handheld's MENU and volume keys (ORUNNERS_HANDHELD_KEYS), and with
# the firmware's SDL2: the executable is linked against a stand-in of its name (sdl2_stub.c) and SDL's headers
# (ORUNNERS_SDL2_INCLUDE, by default the development package in build/downloads/sdl2). The handheld makes
# nothing from the ROM set: its folder gets the assets pack this computer has (build/bin/outrunners.assets,
# which OutRunners writes on its first run from the player's ROM set).

target_compile_definitions(orunners_game_console PUBLIC ORUNNERS_HANDHELD ORUNNERS_HANDHELD_KEYS
	ORUNNERS_CONTROLS_PAGE)

set(ORUNNERS_SDL2_INCLUDE "" CACHE PATH "SDL2's headers (the folder with SDL.h)")
if(NOT ORUNNERS_SDL2_INCLUDE)
	file(GLOB ORUNNERS_SDL2_INCLUDES ${CMAKE_SOURCE_DIR}/build/downloads/sdl2/SDL2-*/include)
	list(GET ORUNNERS_SDL2_INCLUDES 0 ORUNNERS_SDL2_INCLUDE)
endif()
if(NOT EXISTS ${ORUNNERS_SDL2_INCLUDE}/SDL.h)
	message(FATAL_ERROR "No SDL.h in ORUNNERS_SDL2_INCLUDE (${ORUNNERS_SDL2_INCLUDE})")
endif()

# the stand-in for libSDL2-2.0.so.0, beside the build's own files (never in the package)
add_library(SDL2_device SHARED ports/anbernic/sdl2_stub.c)
set_target_properties(SDL2_device PROPERTIES OUTPUT_NAME SDL2-2.0 SUFFIX ".so.0" NO_SONAME ON
	LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sdl2_device)
target_link_options(SDL2_device PRIVATE -Wl,-soname,libSDL2-2.0.so.0)

set(ORUNNERS_ANBERNIC_OUT ${CMAKE_BINARY_DIR}/PORTS)
add_executable(OutRunners ports/sdl2/main.c ports/sdl2/controls.c)
target_include_directories(OutRunners PRIVATE ${ORUNNERS_SDL2_INCLUDE})
target_link_libraries(OutRunners PRIVATE orunners_game_console orunners_sound SDL2_device m)
set_target_properties(OutRunners PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ORUNNERS_ANBERNIC_OUT}/outrunners
	SKIP_BUILD_RPATH ON)    # (no path of this computer's in it: the handheld's loader finds its SDL2)
foreach(config IN ITEMS _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
	set_target_properties(OutRunners PROPERTIES RUNTIME_OUTPUT_DIRECTORY${config} ${ORUNNERS_ANBERNIC_OUT}/outrunners)
endforeach()

# the launcher beside the folder; in it CONTROLS' button pictures (input_prompts.bmp) and this computer's assets
# pack
set(ORUNNERS_ANBERNIC_PACK ${ORUNNERS_BIN_DIR}/outrunners.assets)
add_custom_command(TARGET OutRunners POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/ports/anbernic/OutRunners.sh ${ORUNNERS_ANBERNIC_OUT}
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/ports/sdl2/prompts/input_prompts.bmp
		${ORUNNERS_ANBERNIC_OUT}/outrunners
	VERBATIM)
if(EXISTS ${ORUNNERS_ANBERNIC_PACK})
	add_custom_command(TARGET OutRunners POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ORUNNERS_ANBERNIC_PACK} ${ORUNNERS_ANBERNIC_OUT}/outrunners
		VERBATIM)
else()
	message(STATUS "No assets pack at ${ORUNNERS_ANBERNIC_PACK}: the handheld's folder gets the game alone")
endif()
