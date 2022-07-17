cmake_minimum_required(VERSION 3.18)
project("launcher"
  LANGUAGES CXX
  VERSION "0.1"
)
set(CMAKE_CXX_STANDARD 20)
set(EXECUTABLE_TITLE "launcher")

# Find dependencies
if (UNIX)
  find_package(PkgConfig MODULE REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
  pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
  pkg_check_modules(LIBXML2 REQUIRED IMPORTED_TARGET libxml-2.0)
  pkg_check_modules(INIH REQUIRED IMPORTED_TARGET inih)
  pkg_check_modules(FMT REQUIRED IMPORTED_TARGET fmt)
  pkg_check_modules(SPDLOG REQUIRED IMPORTED_TARGET spdlog)
endif ()

include_directories(${PROJECT_BINARY_DIR})
configure_file(${PROJECT_SOURCE_DIR}/config/config.h.in ${PROJECT_BINARY_DIR}/lconfig.h)
configure_file(${PROJECT_SOURCE_DIR}/config/layout.xml.in ${PROJECT_BINARY_DIR}/layout.xml)
configure_file(${PROJECT_SOURCE_DIR}/config/config.ini.in ${PROJECT_BINARY_DIR}/config.ini)

add_subdirectory(src)

