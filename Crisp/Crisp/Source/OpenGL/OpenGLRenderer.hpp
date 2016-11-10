#pragma once

#include <map>
#include <string>

#include <glad/glad.h>

namespace crisp
{
    class OpenGLRenderer
    {
    public:
        OpenGLRenderer();
        ~OpenGLRenderer() = default;

        GLuint getTechnique() const;

        OpenGLRenderer(const OpenGLRenderer& other) = delete;
        OpenGLRenderer& operator=(const OpenGLRenderer& other) = delete;
        OpenGLRenderer& operator=(OpenGLRenderer&& other) = delete;

    private:

        std::map<std::string, GLuint> m_techniqueMap;
    };
}