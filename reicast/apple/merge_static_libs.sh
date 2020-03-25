#!/bin/bash

# Constants
readonly MACOS_SOURCE_ROOT="${D_OSX}"
readonly LIBOMP_PATH="${MACOS_SOURCE_ROOT}/../deps/libomp/lib/libomp.a"
readonly BUILD_ROOT="${CMAKE_CURRENT_BINARY_DIR}"
readonly LIBSWIRL_PATH="${BUILD_ROOT}/libswirl.a"
readonly MERGED_PATH="${BUILD_ROOT}/libswirl_merged.a"

# Create the combined static lib
echo "--- Merging libomp.a and libswirl.a ---"
libtool -static -o ${MERGED_PATH} ${LIBOMP_PATH} ${LIBSWIRL_PATH} &> /dev/null
rm ${LIBSWIRL_PATH}
mv ${MERGED_PATH} ${LIBSWIRL_PATH}
echo "--- Done ---"
