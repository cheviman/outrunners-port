# Name:        switch.cmake
# Description: the Nintendo Switch build (devkitPro's toolchain): the console game alone, as the pieces of its
#              .nsp (the NSO, main.npdm, the NACP and the icon) and, where the assets pack is, as OutRunners.nro
# Version:     0.1.1
# Created:     30-10-2026
# Modified:    25-11-2026
# Author:      Sebastian Toyos
#
# Included by CMakeLists.txt when the toolchain is the Switch's (aarch64-none-elf-cmake), as SegaSonic's port
# does. The game is the same console build as on a computer, with SDL 3 for the Switch (below: devkitPro's own
# branch of it), its picture fixed and WIDE (ORUNNERS_HANDHELD, ORUNNERS_SCREEN_WIDE), its controls fixed, 2 PLAYERS
# side by side with a second controller (ORUNNERS_TWO_PLAYERS) and no EXIT in its menu (ORUNNERS_SYSTEM_EXIT:
# HOME leaves it). The Switch makes nothing from the ROM set: the game reads its assets pack from its own romfs,
# which a computer fills with the player's (the .nsp, made there with the player's keys; or, where the build has
# the pack beside the executables, build/bin/outrunners.assets, the .nro for the Homebrew Menu, the player's own
# and never shared).

target_compile_definitions(orunners_game_console PUBLIC ORUNNERS_HANDHELD ORUNNERS_SCREEN_WIDE ORUNNERS_TWO_PLAYERS
	ORUNNERS_SYSTEM_EXIT)

# SDL 3 for the Switch (Sebas, 19-11-2026: "Probar SDL3 en Switch"; issue 47). SDL's own Switch port is for those
# under Nintendo's NDA (its docs/README-switch.md), and devkitPro has no package of SDL 3 yet (its portlibs have
# switch-sdl2 alone): SDL is built here from devkitPro's own branch of it (github.com/devkitPro/SDL, switch-sdl-3.4:
# SDL 3.4.0 with libnx's video, sound and controllers; not released, its maintainer: "a couple of tests failing"),
# the commit below by its archive's hash (the one in build/downloads/sdl3-switch-archive where it is there), into
# the game, as TMNT's port has it. A package of devkitPro's (switch-sdl3) is taken instead once there is one
find_package(SDL3 CONFIG QUIET)
if(TARGET SDL3::SDL3)
	set(ORUNNERS_SWITCH_SDL SDL3::SDL3)
else()
	set(ORUNNERS_SWITCH_SDL_COMMIT c329016c0e338ab4397ad930f83330c9fe058348)
	set(ORUNNERS_SWITCH_SDL_SHA256 6047e4787f105ab4a0c6293ee3090ce5d1ebad112fcbe2a3e5a8874b0cc6f709)
	set(ORUNNERS_SWITCH_SDL_URL https://github.com/devkitPro/SDL/archive/${ORUNNERS_SWITCH_SDL_COMMIT}.tar.gz)
	if(EXISTS ${CMAKE_SOURCE_DIR}/build/downloads/sdl3-switch-archive/${ORUNNERS_SWITCH_SDL_COMMIT}.tar.gz)
		set(ORUNNERS_SWITCH_SDL_URL
			${CMAKE_SOURCE_DIR}/build/downloads/sdl3-switch-archive/${ORUNNERS_SWITCH_SDL_COMMIT}.tar.gz)
	endif()
	include(FetchContent)
	FetchContent_Declare(SDL3 URL ${ORUNNERS_SWITCH_SDL_URL} URL_HASH SHA256=${ORUNNERS_SWITCH_SDL_SHA256})
	set(SDL_SHARED OFF CACHE BOOL "" FORCE)
	set(SDL_STATIC ON CACHE BOOL "" FORCE)
	set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
	# On Windows devkitPro's CMake is MSYS2's, which writes SDL's precompiled header with a /d/... path in it, and
	# devkitPro's compiler, a Windows program, does not find it. There SDL's sources get SDL_internal.h included
	# first by the compiler's own option instead (the Switch's files of SDL count on it: without it they are empty)
	if(CMAKE_HOST_SYSTEM_NAME MATCHES "MSYS|CYGWIN|MINGW")
		set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
	endif()
	FetchContent_MakeAvailable(SDL3)
	if(CMAKE_DISABLE_PRECOMPILE_HEADERS)
		target_compile_options(SDL3-static PRIVATE -include SDL_internal.h)
	endif()
	set(ORUNNERS_SWITCH_SDL SDL3::SDL3-static)
endif()

add_executable(OutRunners ports/sdl/main.c ports/sdl/forces.c ports/sdl/controls.c)
target_link_libraries(OutRunners PRIVATE orunners_game_console orunners_sound ${ORUNNERS_SWITCH_SDL})
set_target_properties(OutRunners PROPERTIES SUFFIX ".elf")

# devkitPro's tools: the NACP (the title's name, author and version), the NSO, main.npdm and the .nro
set(ORUNNERS_SWITCH_TOOLS $ENV{DEVKITPRO}/tools/bin)
find_program(ORUNNERS_NACPTOOL nacptool HINTS ${ORUNNERS_SWITCH_TOOLS} REQUIRED)
find_program(ORUNNERS_ELF2NSO elf2nso HINTS ${ORUNNERS_SWITCH_TOOLS} REQUIRED)
find_program(ORUNNERS_NPDMTOOL npdmtool HINTS ${ORUNNERS_SWITCH_TOOLS} REQUIRED)
find_program(ORUNNERS_ELF2NRO elf2nro HINTS ${ORUNNERS_SWITCH_TOOLS} REQUIRED)

set(ORUNNERS_SWITCH_TITLE_ID 05005e6a4f520000)    # (homebrew's range; the .nsp's title; nacptool takes it with no 0x)
set(ORUNNERS_SWITCH_VERSION ${PROJECT_VERSION})
set(ORUNNERS_SWITCH_OUT ${ORUNNERS_BIN_DIR}/switch)
# the HOME menu's icon, 256 x 256: the player's own where this computer has one (local/icon.jpg: local/ holds
# the player's own pictures, never in the repository), else the project's (ports/switch/icon.jpg,
# tools/make_icon.py), else libnx's; the .nsp's maker takes the player's own instead
if(EXISTS ${CMAKE_SOURCE_DIR}/local/icon.jpg)
	set(ORUNNERS_SWITCH_ICON ${CMAKE_SOURCE_DIR}/local/icon.jpg)
elseif(EXISTS ${CMAKE_SOURCE_DIR}/ports/switch/icon.jpg)
	set(ORUNNERS_SWITCH_ICON ${CMAKE_SOURCE_DIR}/ports/switch/icon.jpg)
else()
	set(ORUNNERS_SWITCH_ICON ${NX_DEFAULT_ICON})
endif()

# the .nsp's pieces, which the computer's tool puts together with the player's assets and keys
add_custom_command(OUTPUT ${ORUNNERS_SWITCH_OUT}/main ${ORUNNERS_SWITCH_OUT}/main.npdm ${ORUNNERS_SWITCH_OUT}/control.nacp
		${ORUNNERS_SWITCH_OUT}/icon.jpg
	COMMAND ${CMAKE_COMMAND} -E make_directory ${ORUNNERS_SWITCH_OUT}
	COMMAND ${ORUNNERS_NACPTOOL} --create "OutRunners" "Sebastian Toyos" ${ORUNNERS_SWITCH_VERSION}
		${ORUNNERS_SWITCH_OUT}/control.nacp --titleid=${ORUNNERS_SWITCH_TITLE_ID}
	COMMAND ${ORUNNERS_ELF2NSO} $<TARGET_FILE:OutRunners> ${ORUNNERS_SWITCH_OUT}/main
	COMMAND ${ORUNNERS_NPDMTOOL} ${CMAKE_SOURCE_DIR}/ports/switch/outrunners.json ${ORUNNERS_SWITCH_OUT}/main.npdm
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ORUNNERS_SWITCH_ICON} ${ORUNNERS_SWITCH_OUT}/icon.jpg
	DEPENDS OutRunners ${ORUNNERS_SWITCH_ICON} ${CMAKE_SOURCE_DIR}/ports/switch/outrunners.json
	COMMENT "The .nsp's pieces: main (NSO), main.npdm, control.nacp, icon.jpg"
	VERBATIM)
set(ORUNNERS_SWITCH_OUTPUTS ${ORUNNERS_SWITCH_OUT}/main ${ORUNNERS_SWITCH_OUT}/main.npdm)

# the .nro with the player's assets in its romfs, where this build has them
set(ORUNNERS_SWITCH_PACK ${ORUNNERS_BIN_DIR}/outrunners.assets)
if(EXISTS ${ORUNNERS_SWITCH_PACK})
	set(ORUNNERS_SWITCH_ROMFS ${CMAKE_BINARY_DIR}/switch_romfs)
	add_custom_command(OUTPUT ${ORUNNERS_SWITCH_OUT}/OutRunners.nro
		COMMAND ${CMAKE_COMMAND} -E make_directory ${ORUNNERS_SWITCH_ROMFS}
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ORUNNERS_SWITCH_PACK} ${ORUNNERS_SWITCH_ROMFS}
		COMMAND ${ORUNNERS_ELF2NRO} $<TARGET_FILE:OutRunners> ${ORUNNERS_SWITCH_OUT}/OutRunners.nro
			--icon=${ORUNNERS_SWITCH_ICON} --nacp=${ORUNNERS_SWITCH_OUT}/control.nacp --romfsdir=${ORUNNERS_SWITCH_ROMFS}
		DEPENDS OutRunners ${ORUNNERS_SWITCH_PACK} ${ORUNNERS_SWITCH_OUT}/control.nacp ${ORUNNERS_SWITCH_ICON}
		COMMENT "OutRunners.nro with the assets pack in its romfs (the player's own: never shared)"
		VERBATIM)
	list(APPEND ORUNNERS_SWITCH_OUTPUTS ${ORUNNERS_SWITCH_OUT}/OutRunners.nro)
else()
	message(STATUS "No assets pack at ${ORUNNERS_SWITCH_PACK}: no OutRunners.nro, the .nsp's pieces alone")
endif()
add_custom_target(OutRunners_switch ALL DEPENDS ${ORUNNERS_SWITCH_OUTPUTS})
