cmake_minimum_required(VERSION 3.16)

project(obs-gphoto
    VERSION 0.3.0
    DESCRIPTION "Allows connecting DSLR cameras with OBS Studio through gPhoto on Linux"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# Use PkgConfig for native Debian package discovery
find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBOBS REQUIRED IMPORTED_TARGET libobs)
pkg_check_modules(GPHOTO2 REQUIRED IMPORTED_TARGET libgphoto2)
pkg_check_modules(MAGICK REQUIRED IMPORTED_TARGET MagickCore)

option(ENABLE_UDEV "Enable udev support" ON)
if(ENABLE_UDEV)
    pkg_check_modules(UDEV IMPORTED_TARGET libudev)
endif()

set(SOURCE_FILES
    src/obs-gphoto.c
    src/gphoto-utils.c
    src/gphoto-preview.c
    src/timelapse.c
)

if(UDEV_FOUND)
    list(APPEND SOURCE_FILES src/gphoto-udev.c)
endif()

add_library(obs-gphoto MODULE ${SOURCE_FILES})

target_include_directories(obs-gphoto PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Target-based linking via modern PkgConfig targets
target_link_libraries(obs-gphoto PRIVATE
    PkgConfig::LIBOBS
    PkgConfig::GPHOTO2
    PkgConfig::MAGICK
)

if(UDEV_FOUND)
    target_link_libraries(obs-gphoto PRIVATE PkgConfig::UDEV)
    target_compile_definitions(obs-gphoto PRIVATE HAVE_UDEV)
endif()

set_target_properties(obs-gphoto PROPERTIES
    PREFIX ""
    C_VISIBILITY_PRESET hidden
)

include(GNUInstallDirs)

option(SYSTEM_INSTALL "Install system-wide instead of user config directory" OFF)

if(SYSTEM_INSTALL)
    install(TARGETS obs-gphoto
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins"
    )
else()
    math(EXPR BITS "8 * ${CMAKE_SIZEOF_VOID_P}")
    install(TARGETS obs-gphoto
        DESTINATION "$ENV{HOME}/.config/obs-studio/plugins/obs-gphoto/bin/${BITS}bit"
    )
endif()
