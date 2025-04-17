# big-launcher
**big-launcher** is a work-in-progress HTPC application launcher. The design is loosely based on the [Roku UI](https://www.techhive.com/wp-content/uploads/2022/01/rokuui-100899030-orig.jpg), consisting of a sidebar menu on the left, and selectable apps on the right. This project is intended to be the successor to my other HTPC project, [Flex Launcher](https://github.com/complexlogic/flex-launcher). Compared to Flex Launcher, big-launcher will be more graphically advanced, but less customizable. The program will be written in C++ and utilize SDL for graphics.

"big-launcher" is a temporary working title only; prior to release, a proper title will be chosen. If you have a suggestion for what the program title should be, please reach out with via an Issue or Discussion topic. If you have graphic design skills, I could also use help in creating the application logo/icon.

![Screenshot](https://user-images.githubusercontent.com/95071366/210119196-7925ff34-cec6-4d5f-b580-ef59990e83a2.png)

## Current Status
The program has progressed to the point where it can be used in a working HTPC setup, but it's not ready for release yet. I still need to improve the appearance, stability/error handling, and implement a few more desired features.

Feel free to try it out. The latest Windows development build can be downloaded [here](https://github.com/user-attachments/files/19795906/Windows.build.zip). Linux users should [build from source](#building).

There isn't any documentation written yet. There are two configuration files: `layout.xml` and `config.ini`. The `layout.xml` file defines the menus and application images, and `config.ini` controls the general behavior. You can get a general idea of how the configuration works by studying the default files.

### Development Roadmap
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
- [x] Hotkeys
- [x] Screensaver
- [ ] Clock
- [x] Config/layout file generation
- [ ] Project name chosen
- [ ] Project icon designed
- [x] Windows support
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
```bash
mkdir build && cd build
cmake ..
make
```

The default config references asset files which aren't included in the repo yet because I don't want to permanently bloat the git history with temporary assets from development. You will need to manually download the zip file [here](https://github.com/complexlogic/big-launcher/files/10326572/assets.zip) and extract the contents to your build directory so that the program can find them.
