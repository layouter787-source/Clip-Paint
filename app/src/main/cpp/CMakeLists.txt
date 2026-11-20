cmake_minimum_required(VERSION 3.22.1)
project(clip_paint_native LANGUAGES C CXX)

file(GLOB_RECURSE CLIP_PAINT_CPP CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
file(GLOB_RECURSE CLIP_PAINT_C CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)
# libmypaint includes its GLib compatibility implementation inside mypaint-brush.c.
# Do not compile the optional GLib-only source as a separate translation unit.
list(FILTER CLIP_PAINT_C EXCLUDE REGEX "/third_party/libmypaint/glib/")

add_library(native-lib SHARED
    ${CLIP_PAINT_CPP}
    ${CLIP_PAINT_C}
)

target_compile_features(native-lib PRIVATE cxx_std_20)
set_target_properties(native-lib PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED YES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)
target_compile_options(native-lib PRIVATE
    -O3
    -fexceptions
    -frtti
    -Wno-unused-parameter
)

target_include_directories(native-lib PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/animbrush/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libmypaint"
)

find_library(android-lib android)
find_library(log-lib log)
find_library(egl-lib EGL)
find_library(gles-lib GLESv3)

target_link_libraries(native-lib
    ${android-lib}
    ${log-lib}
    ${egl-lib}
    ${gles-lib}
)
