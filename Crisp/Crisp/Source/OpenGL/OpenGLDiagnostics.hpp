#pragma once

#include <iostream>
#include <iomanip>

#include "glad/glad.h"

namespace crisp
{
    class OpenGLDiagnostics
    {
    public:
        static void enable()
        {
            glDebugMessageCallback(DebugCallback, nullptr);
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        }
        
    private:
        static void APIENTRY DebugCallback(GLenum source, GLenum type, GLuint id,
            GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
        {
            if (type == GL_DEBUG_TYPE_OTHER)
                return;

            std::cout << std::left << std::endl << "----------OpenGL Debug Callback Start----------" << std::endl;
            std::cout << std::setw(10) << "Message: " << message << std::endl;
            std::cout << std::setw(10) << "Type: ";
            switch (type)
            {
            case GL_DEBUG_TYPE_ERROR:
                std::cout << "ERROR";
                break;
            case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
                std::cout << "DEPRECATED_BEHAVIOR";
                break;
            case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
                std::cout << "UNDEFINED_BEHAVIOR";
                break;
            case GL_DEBUG_TYPE_PORTABILITY:
                std::cout << "PORTABILITY";
                break;
            case GL_DEBUG_TYPE_PERFORMANCE:
                std::cout << "PERFORMANCE";
                break;
            case GL_DEBUG_TYPE_OTHER:
                std::cout << "OTHER";
                break;
            }
            std::cout << std::endl;
            std::cout << std::setw(10) << "Id: " << id << std::endl;
            std::cout << std::setw(10) << "Severity: ";
            switch (severity)
            {
            case GL_DEBUG_SEVERITY_LOW:
                std::cout << "LOW";
                break;
            case GL_DEBUG_SEVERITY_MEDIUM:
                std::cout << "MEDIUM";
                break;
            case GL_DEBUG_SEVERITY_HIGH:
                std::cout << "HIGH";
                break;
            }
            std::cout << std::endl;
            std::cout << "----------OpenGL Debug Callback End------------" << std::endl;
        }
    };
}
