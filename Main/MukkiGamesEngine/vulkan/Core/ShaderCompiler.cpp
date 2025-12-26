#include "ShaderCompiler.h"

#include <cstdlib>
#include <filesystem>
#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace ShaderCompiler
{

static fs::path getExecutableDir()
{
#ifdef _WIN32
    std::vector<char> buf(MAX_PATH);
    DWORD len = GetModuleFileNameA(NULL, buf.data(), static_cast<DWORD>(buf.size()));
    if (len == 0 || len == buf.size()) {
        return fs::current_path();
    }
    return fs::path(std::string(buf.data(), len)).parent_path();
#else
    try {
        fs::path p = fs::read_symlink("/proc/self/exe");
        return p.parent_path();
    } catch (...) {
        return fs::current_path();
    }
#endif
}

static fs::path locateCompiledShaders()
{
    // Candidate locations to search for compiled SPIR-V files at runtime.
    std::vector<fs::path> candidates;

    // 1) Working directory /shaders
    candidates.push_back(fs::current_path() / "shaders");

    // 2) Executable dir /shaders
    candidates.push_back(getExecutableDir() / "shaders");

    // 3) Parent directories from executable (up to 5 levels) looking for build tree shaders
    fs::path exeDir = getExecutableDir();
    fs::path p = exeDir;
    for (int i = 0; i < 6; ++i) {       
        candidates.push_back(p / "shaders");
        if (p.has_parent_path()) p = p.parent_path();
        else break;
    }

    // 4) Project build tree (common CMake output directory)
    try {
        // ${CMAKE_BINARY_DIR}/shaders is a standard build output location; attempt common relative paths
        candidates.push_back(fs::current_path() );
        candidates.push_back(getExecutableDir().parent_path());
    } catch (...) {}

    // Try each candidate and return the first that exists
    std::ostringstream tried;
    for (auto &c : candidates) {
        tried << c.string() << "\n";
        std::error_code ec;
        if (c.empty()) continue;
        if (!fs::exists(c, ec) || !fs::is_directory(c, ec)) continue;

        // Ensure the directory actually contains the required compiled SPIR-V
        fs::path vert = c / "vert.spv";
        fs::path frag = c / "frag.spv";
        if (fs::exists(vert, ec) && !ec && fs::exists(frag, ec) && !ec) {
            return fs::canonical(c, ec);
        }
    }

    std::ostringstream os;
    os << "Compiled shader SPIR-V not found. Looked in these locations:\n" << tried.str()
       << "Build shaders at build time with CMake (see project CMakeLists) and ensure the 'shaders' folder is placed next to the executable or in a searched build directory.";
    throw std::runtime_error(os.str());
}

void compileShadersIfNeeded()
{
    // No runtime compilation. Just ensure compiled SPIR-V files are available.
    fs::path shaderBinDir = locateCompiledShaders();

    // Optionally, you can expose the paths or copy them. Here we only validate presence.
    (void)shaderBinDir;
}

} // namespace ShaderCompiler