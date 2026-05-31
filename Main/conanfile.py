from conan import ConanFile


class MukkiGamesEngine(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    requires = [
        "glfw/3.4",
        "imguizmo/cci.20231114",
        "glm/1.0.1",
        "nlohmann_json/3.12.0",
        "tinygltf/2.8.23",
        "stb/cci.20240531",
        "gli/cci.20210515",
    ]
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("imgui/1.90.5-docking", override=True)
