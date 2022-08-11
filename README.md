# big-launcher
**big-launcher** is a work-in-progress HTPC application launcher. The design is loosely based on the [Roku UI](https://www.techhive.com/wp-content/uploads/2022/01/rokuui-100899030-orig.jpg), consisting of a sidebar menu on the left, and selectable apps on the right. This project is intended to be the successor to my other HTPC project, [Flex Launcher](https://github.com/complexlogic/flex-launcher). Compared to Flex Launcher, big-launcher will be more graphically advanced, but less customizable. 

"big-launcher" is a temporary working title only; prior to release, a proper title will be chosen. If you have a suggestion for what the program title should be, please reach out with via an Issue or Discussion topic. If you have graphic design skills, I could also use help in creating the application logo/icon.

The project will be Linux-only while in development, but Windows will be supported in the final released version. The program will be written in C++ and utilize SDL for graphics.

The program has progressed to the point where it can be used in a working HTPC setup, but it's not ready for release yet. I still need to improve the appearance, stability/error handling, and implement a few more desired features.

![Screenshot](https://user-images.githubusercontent.com/95071366/181177048-306a31fb-f5e1-4816-896a-799b690df593.png)

## Roadmap
The following must be accomplished before an initial release can be made:
- [x] Layout parsing
- [x] Config parsing
- [x] Logging
- [x] Graphics rendering
- [x] Animations
- [x] Application card generation
- [x] Command line parsing
- [x] Automatic file searching
- [x] Custom backgrounds
- [x] Application launching
- [x] Audio
- [x] Gamepad controls
- [ ] Hotkeys
- [ ] Screensaver
- [ ] Clock
- [ ] Config/layout file generation
- [ ] Project name chosen
- [ ] Project icon designed
- [ ] Windows support
- [ ] Documentation written

## Building
You need to have the following dependencies installed:
- SDL2
- SDL2_image
- SDL2_ttf
- SDL2_mixer
- libxml2
- inih
- fmt
- spdlog

It has a CMake build system and follows the typical build process:
```
mkdir build && cd build
cmake ..
make
```

The default config references asset files which aren't included in the repo yet because I don't want to permanently bloat the git history with temporary assets from development. You will need to manually download the zip file [here](https://github.com/complexlogic/big-launcher/files/9312187/assets.zip) and extract the contents to your build directory so that the program can find them.
