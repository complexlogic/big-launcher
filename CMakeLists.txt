cmake_minimum_required(VERSION 3.18)
project("launcher"
  LANGUAGES CXX
  VERSION 0.1
)
set(CMAKE_CXX_STANDARD 20)
set(EXECUTABLE_TITLE "launcher")
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif ()
add_compile_definitions("$<$<CONFIG:DEBUG>:DEBUG>")

# Find dependencies
if (UNIX)
  find_package(PkgConfig MODULE REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
  pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
  pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET SDL2_mixer)
  pkg_check_modules(LIBXML2 REQUIRED IMPORTED_TARGET libxml-2.0)
  pkg_check_modules(INIH REQUIRED IMPORTED_TARGET inih)
  pkg_check_modules(FMT REQUIRED IMPORTED_TARGET fmt)
  pkg_check_modules(SPDLOG REQUIRED IMPORTED_TARGET spdlog)
endif ()

include_directories(${PROJECT_BINARY_DIR})

# Config and Layout file generation
set(HTML_USERAGENT "Mozilla/5.0 (Linux; Tizen 2.3; SmartHub; SMART-TV; SmartTV; U; Maple2012) AppleWebKit/538.1+ (KHTML, like Gecko) TV Safari/538.1+")
if (UNIX)
  set(BROWSER_COMMAND "chromium --start-fullscreen --kiosk --user-agent=\"${HTML_USERAGENT}\"")
  set(ICONS_DIR "assets/icons")
  set(CMD_KODI "kodi")
  set(CMD_PLEX "plex-htpc")
  set(CMD_NETFLIX "${BROWSER_COMMAND} netflix.com")
  set(CMD_YOUTUBE "${BROWSER_COMMAND} youtube.com/tv")
  set(CMD_JELLYFIN "jellyfinmediaplayer")
  set(CMD_DISNEYPLUS "${BROWSER_COMMAND} disneyplus.com")
  set(CMD_STEAM "steam")
  set(CMD_RETROARCH "retroarch")
  set(CMD_MINECRAFT "minecraft")
endif ()

configure_file(${PROJECT_SOURCE_DIR}/config/config.h.in ${PROJECT_BINARY_DIR}/lconfig.h)
configure_file(${PROJECT_SOURCE_DIR}/config/layout.xml.in ${PROJECT_BINARY_DIR}/layout.xml)
configure_file(${PROJECT_SOURCE_DIR}/config/config.ini.in ${PROJECT_BINARY_DIR}/config.ini)

add_subdirectory(src)