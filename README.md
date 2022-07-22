# big-launcher
**big-launcher** is a work-in-progress HTPC application launcher. The design is loosely based on the [old Roku UI](https://www.techhive.com/wp-content/uploads/2022/01/rokuui-100899030-orig.jpg), consisting of a sidebar menu on the left, and selectable apps on the right. This project is intended to be the successor to my other HTPC project, [Flex Launcher](https://github.com/complexlogic/flex-launcher). Compared to Flex Launcher, big-launcher will be more graphically advanced, but less customizable. 

"big-launcher" is a temporary working title only; prior to release, a proper title will be chosen. If you have a suggestion for what the program title should be, please reach out with via an Issue or Discussion topic. If you have graphic design skills, I could also use help in creating the application logo/icon.

The project will be Linux-only while in development, but Windows will be supported in the final released version. The program will be written in C++ and utilize SDL for graphics.

So far, all that is accomplished is a basic concept for the UI. It is not currently capable of launching applications and cannot be used in a working HTPC setup yet.

![Screenshot](https://user-images.githubusercontent.com/95071366/180574039-ba2565ea-0bd7-4e43-863e-4dcc551fe081.png)

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
- [ ] Application launching
- [ ] Audio
- [ ] Gamepad controls
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
- libxml2
- inih
- fmt
- spdlog

It has a CMake build system and follows the typical process:
```
mkdir build && cd build
cmake ..
make
```
