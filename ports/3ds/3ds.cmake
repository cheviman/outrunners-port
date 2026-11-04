# Name:        3ds.cmake
# Description: the New Nintendo 3DS build (devkitARM's toolchain): the console game on libctru, as a .3dsx
# Version:     0.1.0
# Created:     03-11-2026
# Modified:    03-11-2026
# Author:      yoshisuga
#
# Included by CMakeLists.txt when the toolchain is the 3DS's. There is no SDL2 there: ports/3ds/main.c is the
# game's whole host, and ports/3ds/gpu.c draws its frames with the PICA200 from the scene video.c describes. Its assets pack (outrunners.assets, made on a computer) is on the SD card

target_compile_definitions(orunners_game_console PUBLIC ORUNNERS_HANDHELD ORUNNERS_SYSTEM_EXIT ORUNNERS_SCREEN_WIDE)
target_compile_definitions(s32runtime PUBLIC S32_FM_SILENT S32_VIDEO_SCENE)
target_compile_options(s32runtime PUBLIC -O3)
target_compile_options(orunners_game_console PUBLIC -O2)
target_link_libraries(s32runtime PUBLIC ctru m)

# (GCC's induction variables make s32_board minus a large constant a loop's base there, an address outside the
# program: a .3dsx cannot relocate it, and Luma's loader stops the whole console)
set_source_files_properties(${ORUNNERS_DECOMPILED_DIR}/system/test_mode.c PROPERTIES COMPILE_OPTIONS -fno-ivopts)

ctr_add_shader_library(outrunners_shader ports/3ds/scene.v.pica)
dkp_add_embedded_binary_library(outrunners_shader_data outrunners_shader)

add_executable(OutRunners ports/3ds/main.c ports/3ds/gpu.c)
target_link_libraries(OutRunners PRIVATE orunners_game_console orunners_sound outrunners_shader_data citro3d)
set(ORUNNERS_3DS_LOG_HOST "" CACHE STRING "A computer's IPv4 address that hears the game's log on UDP 17777 (development)")
if(ORUNNERS_3DS_LOG_HOST)
	target_compile_definitions(OutRunners PRIVATE ORUNNERS_3DS_LOG_HOST="${ORUNNERS_3DS_LOG_HOST}")
endif()
ctr_generate_smdh(OutRunners.smdh NAME "OutRunners" DESCRIPTION "OutRunners" AUTHOR "OutRunners port"
	ICON ${CMAKE_SOURCE_DIR}/ports/3ds/icon.png)
ctr_create_3dsx(OutRunners SMDH OutRunners.smdh)
