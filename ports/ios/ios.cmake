# Name:        ios.cmake
# Description: the iOS and iPadOS build (Xcode, CMake's Xcode generator): OutRunners as an app on SDL 3's
#              UIKit backend, for an iPhone or an iPad (arm64) or the Simulator, and an unsigned .ipa of it
# Version:     0.1.0
# Created:     25-11-2026
# Modified:    25-11-2026
# Author:      Sebastian Toyos
#
# Included by CMakeLists.txt when CMAKE_SYSTEM_NAME is iOS. The game is the console build with its picture
# fixed and WIDE (ORUNNERS_HANDHELD: no first run's window, no VSYNC nor window lines), no EXIT in its menu
# (ORUNNERS_SYSTEM_EXIT: the home gesture leaves it), the touch screen's controls (ORUNNERS_TOUCH,
# ports/sdl/touch.c), the screen whole (ORUNNERS_SCREEN_WIDE: the game's WIDE by the screen's shape, main.c's
# follow_screen, between the sides the notch or the Dynamic Island takes), and the game controllers iOS knows
# (MFi, Xbox, PlayStation, Switch Pro: SDL's driver for Apple's GameController, without SDL_GameControllerDB's
# lines). The app takes its assets out of the player's orunners.zip (MAME's orunners set, World) in its
# Documents folder at its first start (main.c's ios_assets), which the Files app shows as the app's own
# (Info.plist.in: UIFileSharingEnabled, LSSupportsOpeningDocumentsInPlace). What the game writes
# (outrunners.ini, controls.ini, the EEPROM, the bookkeeping, TIME ATTACK's ghosts) and its log,
# outrunners.log, are there too. SDL from its source, the computers' version (CMakeLists.txt's
# ORUNNERS_SDL_VERSION), static; main.c's SDL_main.h starts UIKit. The icon is the project's (Assets.xcassets:
# the car on its wheel, tools/make_icon.py), the launch screen black (LaunchScreen.storyboard), both compiled
# by Xcode. The app is not signed unless ORUNNERS_IOS_TEAM names a development team (a Mac building for its
# own iPhone): the release's .ipa is unsigned, and the player signs it as it is installed (AltStore, SideStore,
# Sideloadly: docs/package/README-ios.txt). Never the App Store: the game is Sega's, and the player brings the
# ROM set.
#
# Usage (macOS, Xcode; for the Simulator, CMAKE_OSX_SYSROOT iphonesimulator, in a build folder of its own):
#   cmake -S . -B build/ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64
#       -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DORUNNERS_REFERENCE_TOOLS=OFF [-DCMAKE_OSX_SYSROOT=iphonesimulator]
#       [-DORUNNERS_BIN_DIR=build/bin-ios] [-DORUNNERS_IOS_TEAM=<team ID>]
#   cmake --build build/ios --config Release --target orunners_ios_games -- CODE_SIGNING_ALLOWED=NO
# The app: ORUNNERS_BIN_DIR/ios/OutRunners.app; for a device, also OutRunners.ipa there (the app inside
# Payload/, zipped).

if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
	message(FATAL_ERROR "The iOS build is the Xcode generator's (-G Xcode): Xcode compiles the app's launch screen and "
		"icon, and puts the bundle together")
endif()

# (ORUNNERS_IOS: the assets out of the player's orunners.zip on the device, main.c's ios_assets)
set(ORUNNERS_IOS_DEFINITIONS ORUNNERS_HANDHELD ORUNNERS_SYSTEM_EXIT ORUNNERS_SCREEN_WIDE ORUNNERS_TOUCH ORUNNERS_IOS)
target_compile_definitions(orunners_game_console PUBLIC ${ORUNNERS_IOS_DEFINITIONS})

set(ORUNNERS_IOS_TEAM "" CACHE STRING "The development team that signs the app (none: unsigned, as the release's)")
set(ORUNNERS_IOS_OUT ${ORUNNERS_BIN_DIR}/ios)
if(CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
	set(ORUNNERS_IOS_SIMULATOR ON)
endif()

# SDL from its source, static (FETCHCONTENT_SOURCE_DIR_SDL3 picks a copy already here). No HIDAPI: iOS's game
# controllers come through Apple's GameController framework, and SDL's HIDAPI there talks Bluetooth itself, which
# needs a permission the app does not ask for
include(FetchContent)
FetchContent_Declare(SDL3 URL ${ORUNNERS_SDL_URL} URL_HASH SHA256=${ORUNNERS_SDL_SHA256})
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

# the app's resources, which Xcode copies into the bundle (compiling the launch screen and the asset catalog): the
# letters of the touch controls and the menus, the launch screen, the icon
set(ORUNNERS_IOS_RESOURCES ports/sdl/font/font.bmp ports/ios/LaunchScreen.storyboard ports/ios/Assets.xcassets)
set_source_files_properties(${ORUNNERS_IOS_RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

set(ORUNNERS_IOS_OUTPUTS "")

# The game of iOS: its app ('target', named 'name', its bundle 'identifier'), the window port's main.c, letters.c,
# touch.c and forces.c and controls.c, on orunners_game_console, orunners_sound and orunners_extract (the assets
# out of the player's orunners.zip on the device, main.c's ios_assets); for a device, its .ipa beside it
function(orunners_ios_game target name identifier)
	add_executable(${target} MACOSX_BUNDLE ports/sdl/main.c ports/sdl/letters.c ports/sdl/touch.c ports/sdl/forces.c
		ports/sdl/controls.c ${ORUNNERS_IOS_RESOURCES})
	target_include_directories(${target} PRIVATE ports/sdl)
	target_link_libraries(${target} PRIVATE orunners_game_console orunners_sound orunners_extract SDL3::SDL3-static)
	target_compile_definitions(${target} PRIVATE ${ORUNNERS_IOS_DEFINITIONS})
	set_target_properties(${target} PROPERTIES OUTPUT_NAME ${name}
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/ports/ios/Info.plist.in
		MACOSX_BUNDLE_BUNDLE_NAME ${name} MACOSX_BUNDLE_GUI_IDENTIFIER ${identifier}
		MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION} MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
		XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${identifier}
		XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"    # (iPhone and iPad)
		XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon)
	foreach(config IN ITEMS "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
		set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY${config} ${ORUNNERS_IOS_OUT})
	endforeach()
	if(ORUNNERS_IOS_TEAM)
		set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ${ORUNNERS_IOS_TEAM}
			XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development")
	else()
		set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO
			XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
	endif()

	set(outputs "")
	if(NOT ORUNNERS_IOS_SIMULATOR)
		# its .ipa: the app in Payload/, zipped (ditto keeps the bundle's permissions), as a sideloading tool takes it
		set(stage ${CMAKE_BINARY_DIR}/ipa/${target})
		set(ipa ${ORUNNERS_IOS_OUT}/${name}.ipa)
		add_custom_command(OUTPUT ${ipa}
			COMMAND ${CMAKE_COMMAND} -E rm -rf ${stage} ${ipa}
			COMMAND ${CMAKE_COMMAND} -E make_directory ${stage}/Payload
			COMMAND ditto $<TARGET_BUNDLE_DIR:${target}> ${stage}/Payload/${name}.app
			COMMAND ${CMAKE_COMMAND} -E chdir ${stage} ditto -c -k --keepParent Payload ${ipa}
			DEPENDS ${target}
			COMMENT "${name}.ipa (unsigned: the player's sideloading tool signs it)"
			VERBATIM)
		list(APPEND outputs ${ipa})
	endif()
	set(ORUNNERS_IOS_OUTPUTS ${ORUNNERS_IOS_OUTPUTS} ${outputs} PARENT_SCOPE)
endfunction()

orunners_ios_game(orunners_ios "OutRunners" io.github.cheviman.outrunners)
add_custom_target(orunners_ios_games ALL DEPENDS ${ORUNNERS_IOS_OUTPUTS})
add_dependencies(orunners_ios_games orunners_ios)
