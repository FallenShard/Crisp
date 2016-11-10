#include "ShaderLoader.hpp"

#include <iostream>
#include <fstream>
#include <vector>

namespace crisp
{
    namespace
    {
        const std::string shaderPath = "Resources/Shaders/";

        std::string fileToString(std::string filePath)
        {
            std::ifstream inputFile(filePath);
            std::string source;

            if (inputFile.is_open())
            {
                std::string line;
                while (std::getline(inputFile, line))
                {
                    source += line + '\n';
                }
                inputFile.close();
            }

            return source;
        }
    }

    GLuint ShaderLoader::load(const std::string& vertexShaderFile, const std::string& fragmentShaderFile)
    {
        GLuint program = glCreateProgram();

        GLuint vertShader = compileShader(GL_VERTEX_SHADER,   vertexShaderFile);
        GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderFile);
        glAttachShader(program, vertShader);
        glAttachShader(program, fragShader);

        glLinkProgram(program);

        GLint isLinked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
        if (isLinked == GL_FALSE)
        {
            std::cerr << "Error linking program!\n";
            GLint maxLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<GLchar> errorLog(maxLength);
            glGetProgramInfoLog(program, maxLength, &maxLength, errorLog.data());

            std::string error(errorLog.begin(), errorLog.end());
            std::cerr << error << std::endl;

            glDeleteProgram(program);
            return -1;
        }

        glDetachShader(program, vertShader);
        glDetachShader(program, fragShader);

        glDeleteShader(vertShader);
        glDeleteShader(fragShader);

        return program;
    }

    GLuint ShaderLoader::compileShader(GLenum type, const std::string& fileName)
    {
        GLuint shader = glCreateShader(type);

        std::string srcString = fileToString(shaderPath + fileName);

        if (srcString == "")
            return -1;

        const GLchar* src = srcString.c_str();
        glShaderSource(shader, 1, &src, nullptr);

        glCompileShader(shader);

        GLint isCompiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
        if (isCompiled == GL_FALSE)
        {
            std::cerr << "\nError compiling shader " + fileName + "!\n";
            GLint maxLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<GLchar> errorLog(maxLength);
            glGetShaderInfoLog(shader, maxLength, &maxLength, errorLog.data());

            std::string error(errorLog.begin(), errorLog.end());
            std::cerr << error << "!\n";

            glDeleteShader(shader);
            return -1;
        }

        return shader;
    }
}