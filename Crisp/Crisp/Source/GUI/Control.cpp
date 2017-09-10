#include "Control.hpp"

#include <iostream>
#include <CrispCore/ConsoleUtils.hpp>

#include "Form.hpp"

namespace crisp::gui
{
    namespace 
    {
        static uint32_t UniqueIdCounter = 0;
    }

    Control::Control(Form* form)
        : m_id("Control_" + std::to_string(UniqueIdCounter++))
        , m_parent(nullptr)
        , m_anchor(Anchor::TopLeft)
        , m_horizontalSizingPolicy(SizingPolicy::Fixed)
        , m_verticalSizingPolicy(SizingPolicy::Fixed)
        , m_parentSizePercent(1.0f, 1.0f)
        , m_position(0.0f)
        , m_sizeHint(50.0f)
        , m_padding(0.0f)
        , m_depthOffset(1.0f)
        , m_color(glm::vec4(1.0f))
        , m_opacity(1.0f)
        , m_scale(1.0f)
        , m_validationFlags(Validation::All)
        , m_form(form)
        , m_renderSystem(form->getRenderSystem())
    {
    }

    Control::~Control()
    {
        std::cout << "Deleting: " << m_id << '\n';
    }

    void Control::setId(std::string&& id)
    {
        m_id = id;
    }

    std::string Control::getId() const
    {
        return m_id;
    }

    void Control::setParent(Control* parent)
    {
        m_parent = parent;
        setValidationFlags(Validation::All);
    }

    Control* Control::getParent() const
    {
        return m_parent;
    }

    void Control::setAnchor(Anchor anchor)
    {
        m_anchor = anchor;
        setValidationFlags(Validation::Geometry);
    }

    Anchor Control::getAnchor() const
    {
        return m_anchor;
    }

    void Control::setSizingPolicy(SizingPolicy horizontal, SizingPolicy vertical, float widthPercent, float heightPercent)
    {
        m_horizontalSizingPolicy = horizontal;
        m_verticalSizingPolicy   = vertical;
        m_parentSizePercent.x    = widthPercent;
        m_parentSizePercent.y    = heightPercent;
        setValidationFlags(Validation::Geometry);
    }

    void Control::setHorizontalSizingPolicy(SizingPolicy sizing, float widthPercent)
    {
        m_horizontalSizingPolicy = sizing;
        m_parentSizePercent.x    = widthPercent;
        setValidationFlags(Validation::Geometry);
    }

    SizingPolicy Control::getHorizontalSizingPolicy() const
    {
        return m_horizontalSizingPolicy;
    }

    void Control::setVerticalSizingPolicy(SizingPolicy sizing, float heightPercent)
    {
        m_verticalSizingPolicy = sizing;
        m_parentSizePercent.y  = heightPercent;
        setValidationFlags(Validation::Geometry);
    }

    SizingPolicy Control::getVerticalSizingPolicy() const
    {
        return m_verticalSizingPolicy;
    }

    void Control::setPosition(const glm::vec2& position)
    {
        m_position = position;
        setValidationFlags(Validation::Geometry);
    }

    glm::vec2 Control::getPosition() const
    {
        return m_position;
    }

    void Control::setSizeHint(const glm::vec2& sizeHint)
    {
        m_sizeHint = sizeHint;
        setValidationFlags(Validation::Geometry);
    }

    glm::vec2 Control::getSize() const
    {
        return { getWidth(), getHeight() };
    }

    float Control::getWidth() const
    {
        if (!m_parent)
            return m_sizeHint.x;

        switch (m_horizontalSizingPolicy)
        {
        case SizingPolicy::FillParent:
            return (m_parent->getWidth() - 2.0f * getParentPadding().x) * m_parentSizePercent.x;

        case SizingPolicy::Fixed:
            // If parent is not fixed (wrapping children or filling its own parent), use size hint
            if (m_parent->getHorizontalSizingPolicy() != SizingPolicy::Fixed)
                return m_sizeHint.x;
            else
                return std::min(m_sizeHint.x, m_parent->getWidth() - 2.0f * getParentPadding().x);

        default:
            ConsoleColorizer console(ConsoleColor::LightRed);
            std::cout << "Attempting to use 'WrapContent' for width on a control which does not support children\n";
            return 0.0f;
        }
    }

    float Control::getHeight() const
    {
        if (!m_parent)
            return m_sizeHint.y;

        switch (m_verticalSizingPolicy)
        {
        case SizingPolicy::FillParent:
            return (m_parent->getHeight() - 2.0f * getParentPadding().y) * m_parentSizePercent.y;

        case SizingPolicy::Fixed:
            // If parent is not fixed (wrapping children or filling its own parent), use size hint
            if (m_parent->getVerticalSizingPolicy() != SizingPolicy::Fixed)
                return m_sizeHint.y;
            else
                return std::min(m_sizeHint.y, m_parent->getHeight() - 2.0f * getParentPadding().y);

        default:
            ConsoleColorizer console(ConsoleColor::LightRed);
            std::cout << "Attempting to use 'WrapContent' for height on a control which does not support children\n";
            return 0.0f;
        }
    }

    void Control::setPadding(glm::vec2&& padding)
    {
        m_padding = padding;
        setValidationFlags(Validation::Geometry);
    }

    glm::vec2 Control::getPadding() const
    {
        return m_padding;
    }

    void Control::setDepthOffset(float depthOffset)
    {
        m_depthOffset = depthOffset;
        setValidationFlags(Validation::Geometry);
    }

    float Control::getDepthOffset() const
    {
        return m_depthOffset;
    }

    void Control::setScale(float scale)
    {
        m_scale = scale;
        setValidationFlags(Validation::Geometry);
    }

    float Control::setScale() const
    {
        return m_scale;
    }

    void Control::setColor(glm::vec4 color)
    {
        m_color.r = color.r;
        m_color.g = color.g;
        m_color.b = color.b;
        m_opacity = color.a;
        setValidationFlags(Validation::Color);
    }

    glm::vec4 Control::getColor() const
    {
        return m_color;
    }

    void Control::setOpacity(float opacity)
    {
        m_opacity = opacity;
        setValidationFlags(Validation::Color);
    }

    float Control::getOpacity() const
    {
        return m_opacity;
    }

    Rect<float> Control::getAbsoluteBounds() const
    {
        return{ m_M[3][0], m_M[3][1], m_M[0][0], m_M[1][1] };
    }

    const glm::mat4& Control::getModelMatrix() const
    {
        return m_M;
    }

    void Control::onMouseMoved(float x, float y)
    {
    }

    void Control::onMouseEntered()
    {
    }

    void Control::onMouseExited()
    {
    }

    void Control::onMousePressed(float x, float y)
    {
    }

    void Control::onMouseReleased(float x, float y)
    {
    }

    void Control::setValidationFlags(ValidationFlags validationFlags)
    {
        m_validationFlags |= validationFlags;
    }

    void Control::clearValidationFlags()
    {
        m_validationFlags = Validation::None;
    }

    ValidationFlags Control::getValidationFlags() const
    {
        return m_validationFlags;
    }

    bool Control::needsValidation()
    {
        return m_validationFlags != Validation::None;
    }

    Control* Control::getControlById(const std::string& id)
    {
        return m_id == id ? this : nullptr;
    }

    unsigned int Control::getRootDistance() const
    {
        return m_parent ? m_parent->getRootDistance() + 1 : 0;
    }

    void Control::printDebugId() const
    {
        for (unsigned int i = 0; i < getRootDistance(); i++)
            std::cout << "    ";

        std::cout << m_id << '\n';
    }

    glm::vec2 Control::getParentAbsolutePosition() const
    {
        return m_parent ? glm::vec2(m_parent->m_M[3][0], m_parent->m_M[3][1]) : glm::vec2(0.0f);
    }

    float Control::getParentAbsoluteDepth() const
    {
        return m_parent ? m_parent->m_M[3][2] : 0.0f;
    }

    glm::vec2 Control::getParentPadding() const
    {
        return m_parent ? m_parent->getPadding() : glm::vec2(0.0f);
    }

    glm::vec2 Control::getParentAbsoluteSize() const
    {
        return m_parent ? m_parent->getSize() : glm::vec2(0.0f);
    }

    float Control::getParentAbsoluteOpacity() const
    {
        return m_parent ? m_parent->m_color.a : 1.0f;
    }

    glm::vec2 Control::getAbsolutePosition() const
    {
        auto parentAbsPos  = getParentAbsolutePosition();
        auto parentPadding = getParentPadding();

        switch (m_anchor)
        {
        default:
        case Anchor::TopLeft:
        {
            return parentAbsPos + parentPadding + m_position;
        }

        case Anchor::TopRight:
        {
            auto parentSize = getParentAbsoluteSize();

            auto size = getSize();

            auto absPos = parentAbsPos;
            absPos.x += parentSize.x - parentPadding.x - size.x - m_position.x;
            absPos.y += parentPadding.y + m_position.y;

            return absPos;
        }

        case Anchor::BottomLeft:
        {
            auto parentSize = getParentAbsoluteSize();

            auto size = getSize();

            auto absPos = parentAbsPos;
            absPos.x += parentPadding.x + m_position.x;
            absPos.y += parentSize.y - parentPadding.y - size.y - m_position.y;

            return absPos;
        }

        case Anchor::BottomRight:
        {
            auto parentSize = getParentAbsoluteSize();
            auto size = getSize();

            return parentAbsPos + parentSize - parentPadding - size - m_position;
        }

        case Anchor::Center:
        {
            auto parentSize = getParentAbsoluteSize();
            auto size = getSize();

            auto absPos = parentAbsPos;

            if (m_horizontalSizingPolicy == SizingPolicy::FillParent)
                absPos.x += parentPadding.x;
            else
                absPos.x += (parentSize.x - size.x) / 2.0f;

            if (m_verticalSizingPolicy == SizingPolicy::FillParent)
                absPos.y += parentPadding.y;
            else
                absPos.y += (parentSize.y - size.y) / 2.0f;

            return absPos + m_position;
        }

        case Anchor::CenterTop:
        {
            auto parentSize = getParentAbsoluteSize();
            auto size = getSize();

            auto absPos = parentAbsPos;

            if (m_horizontalSizingPolicy == SizingPolicy::FillParent)
                absPos.x += parentPadding.x;
            else
                absPos.x += (parentSize.x - size.x) / 2.0f;

            absPos.y += parentPadding.y;
            absPos += m_position;

            return absPos;
        }

        case Anchor::CenterLeft:
        {
            auto parentSize = getParentAbsoluteSize();
            auto size = getSize();

            auto absPos = parentAbsPos;

            absPos.x += parentPadding.x;

            if (m_verticalSizingPolicy == SizingPolicy::FillParent)
                absPos.y += parentPadding.y;
            else
                absPos.y += (parentSize.y - size.y) / 2.0f;

            absPos += m_position;

            return absPos;
        }

        case Anchor::CenterBottom:
        {
            auto parentSize = getParentAbsoluteSize();
            auto size = getSize();

            auto absPos = parentAbsPos;

            if (m_horizontalSizingPolicy == SizingPolicy::FillParent)
                absPos.x += parentPadding.x;
            else
                absPos.x += (parentSize.x - size.x) / 2.0f;

            absPos.x += m_position.x;
            absPos.y += parentSize.y - parentPadding.y - size.y - m_position.y;

            return absPos;
        }

        case Anchor::CenterRight:
        {
            auto parentSize = getParentAbsoluteSize();
            auto size = getSize();

            auto absPos = parentAbsPos;

            if (m_verticalSizingPolicy == SizingPolicy::FillParent)
                absPos.y += parentPadding.y;
            else
                absPos.y += (parentSize.y - size.y) / 2.0f;

            absPos.x += parentSize.x - parentPadding.x - size.x - m_position.x;
            absPos.y += m_position.y;

            return absPos;
        }
        }
    }

    float Control::getAbsoluteDepth() const
    {
        return getParentAbsoluteDepth() + m_depthOffset;
    }
}