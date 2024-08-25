# imx
An experimental CPU only backend for ImGui built with Blend2d and X11

Please visit and support these amazing products this library is built upon:

[Blend2d](https://blend2d.com)

[Dear ImGui](https://github.com/ocornut/imgui)

# Building IMX
Conan is used to manage project dependencies. Documentation on using conan can be found at https://docs.conan.io/2

With conan installed on your system simply configure and build and required dependencies will be downloader or built for your system

cmake . -B build -DCMAKE_BUILD_TYPE=Release

cmake --build build

