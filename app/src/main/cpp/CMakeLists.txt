cmake_minimum_required(VERSION 3.22.1)
project(clip_paint_native LANGUAGES C CXX)

file(GLOB_RECURSE CLIP_PAINT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(native-lib SHARED ${CLIP_PAINT_SOURCES})

target_compile_features(native-lib PRIVATE cxx_std_20)
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
