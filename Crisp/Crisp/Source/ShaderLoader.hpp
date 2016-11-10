#pragma once

#include <string>

#include <glad/glad.h>

namespace crisp
{
    class ShaderLoader
    {
    public:
        ShaderLoader() = default;

        GLuint load(const std::string& vertexShaderFile, const std::string& fragmentShaderFile);

    private:
        GLuint compileShader(GLenum type, const std::string& fileName);
    };
}