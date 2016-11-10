#include "Panel.hpp"

namespace crisp
{
    namespace gui
    {
        glm::vec4 Panel::getColor() const
        {
            return glm::vec4(0.1f, 0.1f, 0.1f, 0.8f);
        }

        void Panel::draw(const DrawingVisitor& visitor) const
        {
            visitor.draw(*this);

            ControlGroup::draw(visitor);
        }
    }
}