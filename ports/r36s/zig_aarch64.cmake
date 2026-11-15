# Name:        zig_aarch64.cmake
# Description: CMake's toolchain for the R36S and its RK3326 kin on ArkOS (aarch64 Linux, Ubuntu 19.10's glibc 2.30):
#              ports/anbernic's, Zig's C compiler, for that glibc
#
# Usage: cmake -S . -B build/r36s -G Ninja -DCMAKE_TOOLCHAIN_FILE=ports/r36s/zig_aarch64.cmake
#        -DORUNNERS_ANBERNIC=ON -DORUNNERS_R36S=ON [-DZIG=path/to/zig]

set(ORUNNERS_ZIG_GLIBC 2.30)
include(${CMAKE_CURRENT_LIST_DIR}/../anbernic/zig_aarch64.cmake)
