from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps


class ImxRecipe(ConanFile):
    name = "imx"
    version = "1.0.0"
    package_type = "application"

    # Optional metadata
    license = "MIT"
    author = "benny.edlund@gmail.com"
    url = "https://github.com/benny-edlund/imx"
    description = "A backend for ImGui built on blend2d & X11"
    topics = ("imgui", "X11", "blend2d")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "lib/*"

    def requirements(self):
        self.requires("imgui/1.90.1")
        self.requires("blend2d/0.10.5")
        self.requires("glfw/3.3.8")
        self.requires("fmt/10.2.1")
        self.requires("gsl/2.7.1")
        self.requires("tracy/0.10")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    

    
