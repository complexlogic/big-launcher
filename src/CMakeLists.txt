set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}")
set(SOURCES "main.cpp" "layout.cpp" "image.cpp" "sound.cpp" "util.cpp" "screensaver.cpp")
if (UNIX)
  add_executable(${EXECUTABLE_TITLE} ${SOURCES})
  target_link_libraries(${EXECUTABLE_TITLE} 
    PkgConfig::SDL2
    PkgConfig::SDL2_IMAGE
    PkgConfig::SDL2_TTF
    PkgConfig::SDL2_MIXER
    PkgConfig::LIBXML2 
    PkgConfig::INIH
    PkgConfig::FMT
    PkgConfig::SPDLOG
  )
elseif (WIN32)
  set(MANIFEST_FILE "${PROJECT_BINARY_DIR}/${EXECUTABLE_TITLE}.manifest")
  add_executable(${EXECUTABLE_TITLE} WIN32 ${SOURCES} ${MANIFEST_FILE})
  set_property(TARGET ${EXECUTABLE_TITLE} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
  target_link_libraries(${EXECUTABLE_TITLE} 
  $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
  $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
	$<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf>,SDL2_ttf::SDL2_ttf,SDL2_ttf::SDL2_ttf-static> 
	$<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer>,SDL2_mixer::SDL2_mixer,SDL2_mixer::SDL2_mixer-static>
	spdlog::spdlog
	LibXml2::LibXml2
	unofficial::inih::libinih
	${GETOPT} 
	PowrProf
  )
  target_include_directories(${EXECUTABLE_TITLE} PUBLIC ${GETOPT_INCLUDE_DIR})
endif ()

add_subdirectory("platform")
target_link_libraries(${EXECUTABLE_TITLE} platform)
