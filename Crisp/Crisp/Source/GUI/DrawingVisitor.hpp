#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace crisp
{
    namespace gui
    {
        class Button;
        class CheckBox;
        class Label;
        class Panel;

        class DrawingVisitor
        {
        public:
            DrawingVisitor();
            ~DrawingVisitor();

            const glm::mat4& getProjectionMatrix() const;
            void setProjectionMarix(glm::mat4&& projMatrix);

            void draw(const Panel& panel) const;
            void draw(const Button& button) const;
            void draw(const CheckBox& checkBox) const;
            void draw(const Label& label) const;

        private:
            void setupGuiShaders();
            void initButtonResources();
            void initCheckBoxResources();
            void initLabelResources();

            glm::mat4 m_P;

            GLuint m_textProg;
            GLuint m_textMvpUnif;
            GLuint m_textColorUnif;
            GLuint m_textZUnif;

            GLuint m_quadTexProg;
            GLuint m_quadTexMvpUnif;
            GLuint m_quadTexTexCoordScaleUnif;
            GLuint m_quadTexTexCoordOffsetUnif;
            GLuint m_quadTexColorUnif;
            GLuint m_quadTexZUnif;

            GLuint m_quadColProg;
            GLuint m_quadColMvpUnif;
            GLuint m_quadColOffsetUnif;
            GLuint m_quadColZUnif;

            GLuint m_quadUniformColProg;
            GLuint m_quadUniformColMvpUnif;
            GLuint m_quadUniformColColorUnif;
            GLuint m_quadUniformZUnif;

            GLuint m_quadVao;
            GLuint m_quadVbo;
            GLuint m_quadIbo;
            
            GLuint m_buttonCbo;

            GLuint m_checkBoxTex;
        };
    }
}