from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeToolchain

class ConanApplication(ConanFile):
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeConfigDeps"

    default_options = {
        "grpc/*:php_plugin": False,
        "grpc/*:node_plugin": False,
        "grpc/*:ruby_plugin": False,
        "grpc/*:csharp_plugin": False,
        "grpc/*:python_plugin": False,
        "grpc/*:objective_c_plugin": False,
        "mysql-connector-cpp/*:shared": True
    }
    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.user_presets_path = False
        tc.generate()

    def requirements(self):
        self.requires("protobuf/6.32.1", override=True)
        self.requires("abseil/20250814.1", override=True)
        self.requires("prometheus-cpp/1.3.0")
        self.requires("benchmark/1.9.4")
        self.requires("gtest/1.17.0")
        self.requires("rapidjson/1.1.0")
        self.requires("spdlog/1.17.0")
        self.requires("grpc/1.72.0")

    def build_requirements(self):
        self.tool_requires("ninja/1.13.2")
        self.tool_requires("protobuf/6.32.1")