#pragma once

#include "ControlGroup.hpp"

namespace crisp
{
    namespace gui
    {
        class Panel : public ControlGroup
        {
        public:

            glm::vec4 getColor() const;

            virtual void draw(const DrawingVisitor& visitor) const override;

        private:

        };
    }
}