# Building The Code

## Requirements

Phoenix Doom requires the following in order to build:
    - A compiler fully supporting ISO C++17 and the C++17 standard library. One exception: I have made workarounds on MacOS due to the lack of support for the `<filesystem>` header (except on the very latest XCode and MacOS).
    - CMake 3.14 or greater to generate your IDE specific project files, or makefiles/build-scripts etc. The project might possibly be made to work with older CMakes if you reduce the requirement in `CMakeLists.txt`, but I offer no guarantee. 3.14 is what I used during development so I can be sure this works.
    - SDKs and libraries relevant the environment you are building for. For example on Windows you will need the Windows SDK, and on Linux you will need to download development headers and static libs for the various library dependencies (The 'X' Window system etc.).

## Tested build environments and IDEs

I have successfully built against the following. Other configurations may be possible of course, but these are the ones that I have tested:
    - Windows: Visual Studio 2017 and 2019, targeting the MSVC compiler. This is my primary development environment and is by far the most well tested. I have successfully built both 64-bit and 32-bit binaries also but I **strongly** recommend you build 64-bit binaries for performance reasons - some of the code assumes a 64-bit architecture.
    - Windows: QT Creator, targeting GCC 9.1/MinGW-64.
    - MacOS: Xcode 10 and 11 targeting the Apple Clang compiler.
    - Linux: makefiles targeting GCC 9.1.
