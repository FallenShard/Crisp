#include "DrawingVisitor.hpp"

#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "Core/Image.hpp"
#include "ShaderLoader.hpp"

#include "Button.hpp"
#include "Label.hpp"
#include "CheckBox.hpp"
#include "Panel.hpp"

namespace crisp
{
    namespace gui
    {
        DrawingVisitor::DrawingVisitor()
        {
            setupGuiShaders();

            std::vector<glm::vec2> vertices =
            {
                {0.0f, 0.0f},
                {1.0f, 0.0f},
                {1.0f, 1.0f},
                {0.0f, 1.0f}
            };
            glCreateBuffers(1, &m_quadVbo);
            glNamedBufferStorage(m_quadVbo, vertices.size() * sizeof(glm::vec2), vertices.data(), GL_MAP_WRITE_BIT);

            std::vector<glm::u16vec3> faces =
            {
                {0, 1, 2},
                {0, 2, 3}
            };
            glCreateBuffers(1, &m_quadIbo);
            glNamedBufferStorage(m_quadIbo, faces.size() * sizeof(glm::u16vec3), faces.data(), GL_MAP_WRITE_BIT);

            std::vector<glm::vec3> buttonColors =
            {
                {0.4f, 0.4f, 0.4f},
                {0.4f, 0.4f, 0.4f},
                {0.15f, 0.15f, 0.15f},
                {0.15f, 0.15f, 0.15f}
            };
            glCreateBuffers(1, &m_buttonCbo);
            glNamedBufferStorage(m_buttonCbo, buttonColors.size() * sizeof(glm::vec3), buttonColors.data(), GL_MAP_WRITE_BIT);

            glCreateVertexArrays(1, &m_quadVao);

            glEnableVertexArrayAttrib(m_quadVao, 0);
            glVertexArrayAttribFormat(m_quadVao, 0, 2, GL_FLOAT, GL_FALSE, 0);
            glVertexArrayAttribBinding(m_quadVao, 0, 0);
            glVertexArrayVertexBuffer(m_quadVao, 0, m_quadVbo, 0, sizeof(glm::vec2));
            
            glEnableVertexArrayAttrib(m_quadVao, 1);
            glVertexArrayAttribFormat(m_quadVao, 1, 3, GL_FLOAT, GL_FALSE, 0);
            glVertexArrayAttribBinding(m_quadVao, 1, 1);
            glVertexArrayVertexBuffer(m_quadVao, 1, m_buttonCbo, 0, sizeof(glm::vec3));
            
            glVertexArrayElementBuffer(m_quadVao, m_quadIbo);

            m_P = glm::ortho(0.0f, 960.0f, 540.0f, 0.0f, 0.5f, 32.5f);

            auto image = std::make_unique<Image>("Resources/Textures/Gui/check-box.png");
            glGenTextures(1, &m_checkBoxTex);
            glBindTexture(GL_TEXTURE_2D, m_checkBoxTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image->getWidth(), image->getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, image->getData());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        DrawingVisitor::~DrawingVisitor()
        {
            glDeleteVertexArrays(1, &m_quadVao);
            glDeleteBuffers(1, &m_quadVbo);
            glDeleteBuffers(1, &m_quadIbo);
            glDeleteBuffers(1, &m_buttonCbo);
        }

        const glm::mat4& DrawingVisitor::getProjectionMatrix() const
        {
            return m_P;
        }

        void DrawingVisitor::setProjectionMarix(glm::mat4&& projMatrix)
        {
            m_P = projMatrix;
        }

        void DrawingVisitor::draw(const Panel& panel) const
        {
            glUseProgram(m_quadUniformColProg);

            glUniformMatrix4fv(m_quadUniformColMvpUnif, 1, GL_FALSE, glm::value_ptr(m_P * panel.getModelMatrix()));
            glUniform4fv(m_quadUniformColColorUnif, 1, glm::value_ptr(panel.getColor()));
            glUniform1f(m_quadUniformZUnif, panel.getDepth());

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBindVertexArray(m_quadVao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
            glBindVertexArray(0);
            glDisable(GL_BLEND);
        }

        void DrawingVisitor::draw(const Button& button) const
        {
            glUseProgram(m_quadColProg);

            glUniformMatrix4fv(m_quadColMvpUnif, 1, GL_FALSE, glm::value_ptr(m_P * button.getModelMatrix()));
            glUniform4fv(m_quadColOffsetUnif, 1, glm::value_ptr(button.getColorOffset()));
            glUniform1f(m_quadColZUnif, button.getDepth());

            glBindVertexArray(m_quadVao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
            glBindVertexArray(0);
        }

        void DrawingVisitor::draw(const CheckBox& checkBox) const
        {
            glUseProgram(m_quadTexProg);

            glBindTextureUnit(0, m_checkBoxTex);

            glUniformMatrix4fv(m_quadTexMvpUnif, 1, GL_FALSE, glm::value_ptr(m_P * checkBox.getModelMatrix()));
            glUniform2f(m_quadTexTexCoordScaleUnif, 0.5f, 1.0f);
            glUniform2f(m_quadTexTexCoordOffsetUnif, checkBox.isChecked() ? 0.5f : 0.0f, 0.0f);
            glUniform4fv(m_quadTexColorUnif, 1, glm::value_ptr(checkBox.getRenderedColor()));
            glUniform1f(m_quadTexZUnif, checkBox.getDepth());

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBindVertexArray(m_quadVao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
            glBindVertexArray(0);
            glDisable(GL_BLEND);
        }

        void DrawingVisitor::draw(const Label& label) const
        {
            glUseProgram(m_textProg);

            glBindTextureUnit(0, label.getFont().textureId);

            glUniformMatrix4fv(m_textMvpUnif, 1, false, glm::value_ptr(m_P * label.getModelMatrix()));
            glUniform4fv(m_textColorUnif, 1, glm::value_ptr(label.getColor()));
            glUniform1f(m_textZUnif, label.getDepth());

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            label.drawGeometry();
            glDisable(GL_BLEND);
        }

        void DrawingVisitor::initButtonResources()
        {
        }

        void DrawingVisitor::initCheckBoxResources()
        {

        }

        void DrawingVisitor::initLabelResources()
        {

        }

        void DrawingVisitor::setupGuiShaders()
        {
            ShaderLoader loader;

            m_textProg = loader.load("gui-text-vert.glsl", "gui-text-frag.glsl");
            m_textMvpUnif = glGetUniformLocation(m_textProg, "MVP");
            m_textColorUnif = glGetUniformLocation(m_textProg, "color");
            m_textZUnif = glGetUniformLocation(m_textProg, "z");
            glProgramUniform1i(m_textProg, glGetUniformLocation(m_textProg, "atlas"), 0);

            m_quadTexProg = loader.load("gui-quad-tex-vert.glsl", "gui-quad-tex-frag.glsl");
            m_quadTexMvpUnif = glGetUniformLocation(m_quadTexProg, "MVP");
            m_quadTexTexCoordScaleUnif = glGetUniformLocation(m_quadTexProg, "texCoordScale");
            m_quadTexTexCoordOffsetUnif = glGetUniformLocation(m_quadTexProg, "texCoordOffset");
            m_quadTexColorUnif = glGetUniformLocation(m_quadTexProg, "color");
            m_quadTexZUnif = glGetUniformLocation(m_quadTexProg, "z");
            glProgramUniform1i(m_quadTexProg, glGetUniformLocation(m_quadTexProg, "tex"), 0);

            m_quadColProg = loader.load("gui-quad-col-vert.glsl", "gui-quad-col-frag.glsl");
            m_quadColMvpUnif = glGetUniformLocation(m_quadColProg, "MVP");
            m_quadColOffsetUnif = glGetUniformLocation(m_quadColProg, "offset");
            m_quadColZUnif = glGetUniformLocation(m_quadColProg, "z");

            m_quadUniformColProg = loader.load("gui-quad-unif-col-vert.glsl", "gui-quad-unif-col-frag.glsl");
            m_quadUniformColMvpUnif = glGetUniformLocation(m_quadUniformColProg, "MVP");
            m_quadUniformColColorUnif = glGetUniformLocation(m_quadUniformColProg, "color");
            m_quadUniformZUnif = glGetUniformLocation(m_quadUniformColProg, "z");
        }
    }
}
