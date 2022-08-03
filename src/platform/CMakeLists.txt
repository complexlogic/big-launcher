if (UNIX)
  add_library(platform "unix.cpp")
  target_link_libraries(platform PkgConfig::SDL2 PkgConfig::SPDLOG)
endif ()