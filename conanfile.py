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
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = "CMakeLists.txt", "lib/*"
    options = {"shared": [True, False]}
    default_options = {"shared": False}

    def requirements(self):
        self.requires("imgui/1.91.0")
        self.requires("blend2d/0.11.4")
        self.requires("glfw/3.3.8")
        self.requires("fmt/10.2.1")
        self.requires("gsl/2.7.1")
        self.requires("tracy/0.11.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()
    
    def configure(self):
        self.options["imgui"].shared = True

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    

    
