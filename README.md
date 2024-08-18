# imx
An experimental ImGui backend built with Blend2d and X11

# Building
Conan is used to manage project dependencies. Documentation on using conan can be found at https://docs.conan.io/2

With conan installed on your system simply configure and build and required dependencies will be downloader or built for your system
cmake . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

