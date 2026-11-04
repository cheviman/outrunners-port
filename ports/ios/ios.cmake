# Name:        ios.cmake
# Description: the iOS build (CMAKE_SYSTEM_NAME iOS, Xcode's SDKs): the console game in OutRunners.app with
#              SDL2 from its source, and the .ipa around it
# Version:     0.1.0
# Created:     03-11-2026
# Modified:    03-11-2026
# Author:      yoshisuga
#
# Included by CMakeLists.txt. The game is Android's: the picture fixed and WIDE (ORUNNERS_HANDHELD,
# ORUNNERS_SCREEN_WIDE), no EXIT in its menu (ORUNNERS_SYSTEM_EXIT: the system leaves it) and the touch screen's
# controls (ORUNNERS_TOUCH). Unlike Android's it carries nothing of the ROM set: the first run takes the assets
# out of it on the phone (ports/sdl2/first_run.c), so the .app and the .ipa have nothing of the game's data.
# The simulator's .app is signed ad hoc here; the device's .ipa comes unsigned (a player's own profile signs it,
# ports/ios/README.md)

target_compile_definitions(orunners_game_console PUBLIC ORUNNERS_HANDHELD ORUNNERS_SYSTEM_EXIT ORUNNERS_SCREEN_WIDE
	ORUNNERS_TOUCH ORUNNERS_IOS)

enable_language(OBJC)

set(ORUNNERS_IOS_BUNDLE_ID "io.github.yoshisuga.outrunners" CACHE STRING "The app's bundle identifier")
if(CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
	set(ORUNNERS_IOS_PLATFORM iPhoneSimulator)
	set(ORUNNERS_IOS_ACTOOL_PLATFORM iphonesimulator)
else()
	set(ORUNNERS_IOS_PLATFORM iPhoneOS)
	set(ORUNNERS_IOS_ACTOOL_PLATFORM iphoneos)
endif()

# SDL2 from its source, as the releases' (FETCHCONTENT_SOURCE_DIR_SDL2 picks a copy already here)
include(FetchContent)
FetchContent_Declare(SDL2 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-2.32.10
	GIT_SHALLOW TRUE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2)

# (each build folder its own .app: the simulator's and the device's)
foreach(config IN ITEMS "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY${config} ${CMAKE_BINARY_DIR}/bin)
endforeach()
add_executable(OutRunners MACOSX_BUNDLE ports/sdl2/main.c ports/sdl2/controls.c ports/sdl2/first_run.c
	ports/sdl2/letters.c ports/sdl2/touch.c ports/ios/ios.m)
target_include_directories(OutRunners PRIVATE ports/ios ports/sdl2)
target_link_libraries(OutRunners PRIVATE orunners_game_console orunners_sound orunners_extract SDL2::SDL2-static)
if(TARGET SDL2::SDL2main)
	target_link_libraries(OutRunners PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(OutRunners PRIVATE "-framework CoreMotion" "-framework UniformTypeIdentifiers")
set_source_files_properties(ports/ios/ios.m PROPERTIES COMPILE_OPTIONS -fobjc-arc)
configure_file(ports/ios/Info.plist.in ${CMAKE_BINARY_DIR}/Info.plist @ONLY)
set_target_properties(OutRunners PROPERTIES
	MACOSX_BUNDLE_INFO_PLIST ${CMAKE_BINARY_DIR}/Info.plist)

# The icon: the asset catalog actool compiles, its one 1024 x 1024 picture without transparency (iOS's), and
# what the catalog adds to the Info.plist
set(ORUNNERS_IOS_ICONS ${CMAKE_BINARY_DIR}/ios/Assets.xcassets/AppIcon.appiconset)
file(MAKE_DIRECTORY ${ORUNNERS_IOS_ICONS} ${CMAKE_BINARY_DIR}/ios/compiled)
file(WRITE ${ORUNNERS_IOS_ICONS}/Contents.json
	"{\"images\":[{\"filename\":\"icon.png\",\"idiom\":\"universal\",\"platform\":\"ios\",\"size\":\"1024x1024\"}],"
	"\"info\":{\"author\":\"xcode\",\"version\":1}}\n")
add_custom_command(TARGET OutRunners POST_BUILD
	COMMAND sips -s format jpeg -z 1024 1024 ${CMAKE_SOURCE_DIR}/ports/sdl2/icon/outrunners.png
		--out ${CMAKE_BINARY_DIR}/ios/icon.jpg
	COMMAND sips -s format png ${CMAKE_BINARY_DIR}/ios/icon.jpg --out ${ORUNNERS_IOS_ICONS}/icon.png
	COMMAND xcrun actool ${CMAKE_BINARY_DIR}/ios/Assets.xcassets --compile $<TARGET_BUNDLE_CONTENT_DIR:OutRunners>
		--platform ${ORUNNERS_IOS_ACTOOL_PLATFORM} --minimum-deployment-target ${CMAKE_OSX_DEPLOYMENT_TARGET}
		--target-device iphone --target-device ipad --app-icon AppIcon
		--output-partial-info-plist ${CMAKE_BINARY_DIR}/ios/icon.plist
	COMMAND /usr/libexec/PlistBuddy -c "Merge ${CMAKE_BINARY_DIR}/ios/icon.plist"
		$<TARGET_BUNDLE_CONTENT_DIR:OutRunners>/Info.plist
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/ports/sdl2/font/font.bmp $<TARGET_BUNDLE_CONTENT_DIR:OutRunners>
	COMMENT "The app's icon and letters"
	VERBATIM)
if(ORUNNERS_IOS_PLATFORM STREQUAL iPhoneSimulator)
	add_custom_command(TARGET OutRunners POST_BUILD
		COMMAND codesign --force --sign - --timestamp=none $<TARGET_BUNDLE_DIR:OutRunners>
		COMMENT "Signed for the simulator"
		VERBATIM)
endif()

# The .ipa, for a player's own profile
set(ORUNNERS_IPA ${CMAKE_BINARY_DIR}/bin/OutRunners.ipa)
add_custom_command(OUTPUT ${ORUNNERS_IPA}
	COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/ios/Payload ${ORUNNERS_IPA}
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/ios/Payload
	COMMAND cp -R $<TARGET_BUNDLE_DIR:OutRunners> ${CMAKE_BINARY_DIR}/ios/Payload/
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/ios zip -qry ${ORUNNERS_IPA} Payload
	DEPENDS OutRunners
	COMMENT "The .ipa: ${ORUNNERS_IPA}"
	VERBATIM)
add_custom_target(outrunners_ipa DEPENDS ${ORUNNERS_IPA})
