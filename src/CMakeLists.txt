set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}")
if (UNIX)
  add_executable(${EXECUTABLE_TITLE} "main.cpp" "layout.cpp" "image.cpp" "util.cpp")
  target_link_libraries(${EXECUTABLE_TITLE} 
    PkgConfig::SDL2
    PkgConfig::SDL2_IMAGE
    PkgConfig::SDL2_TTF
    PkgConfig::LIBXML2 
    PkgConfig::INIH
    PkgConfig::FMT
    PkgConfig::SPDLOG
  )
endif ()
