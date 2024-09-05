
#define SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS ON
#include <fstream>
#include <iostream>
#include <span>
#include <spirv_cross/spirv_glsl.hpp>
#include <string>

using namespace std::string_literals;

static std::vector<uint32_t> readSpirV(std::string path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "did not find file " << path << std::endl;
        return {};
    }
    size_t size = (size_t)file.tellg();
    std::vector<uint32_t> ret(size / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char *>(ret.data()), size);
    file.close();
    return ret;
}

static bool writeShader(std::string path, std::string content)
{
    std::fstream file(path, std::ios::trunc | std::ios::out);
    if (!file.is_open()) {
        return false;
    }
    file << content;
    file.close();
    return true;
}

int main(int argc, char **argv)
{
    const auto arguments = std::span(argv, argc - 1);
    const char *input = arguments[1];
    const char *outputBase = arguments[2];
    const std::string spirvPath = outputBase + ".spirv"s;

    std::string command = "glslc "s + input + " -o "s + spirvPath;
    if (int err = system(command.c_str()); err != 0) {
        std::cerr << "processing " << input << " failed with error " << err << "\n";
        return -1;
    }

    const auto data = readSpirV(spirvPath);
    if (data.empty()) {
        std::cerr << "reading spirv failed!\n";
        return -1;
    }
    spirv_cross::CompilerGLSL compiler(data);

    // create OpenGL shaders for desktop and non-desktop
    // both only version 2.0 because we don't really use more shader features atm
    {
        spirv_cross::CompilerGLSL::Options options;
        options.version = 200;
        options.es = true;
        options.emit_uniform_buffer_as_plain_uniforms = true;
        compiler.set_common_options(options);
        if (!writeShader(outputBase + ".gles"s, compiler.compile())) {
            std::cerr << "writing shader file for " << input << " failed\n";
            return -1;
        }
    }
    {
        spirv_cross::CompilerGLSL::Options options;
        options.version = 200;
        options.es = false;
        options.emit_uniform_buffer_as_plain_uniforms = true;
        compiler.set_common_options(options);
        if (!writeShader(outputBase + ".desktopgl"s, compiler.compile())) {
            std::cerr << "writing shader file for " << input << " failed\n";
            return -1;
        }
    }
    return 0;
}
