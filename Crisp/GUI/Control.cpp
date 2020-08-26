#include "Control.hpp"

#include <CrispCore/Log.hpp>

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
        , m_origin(Origin::TopLeft)
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

    void Control::setOrigin(Origin origin)
    {
        m_origin = origin;
        setValidationFlags(Validation::Geometry);
    }

    Origin Control::getOrigin() const
    {
        return m_origin;
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
            logError("Attempting to use 'WrapContent' for width on a control which does not support children.");
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
            logError("Attempting to use 'WrapContent' for height on a control which does not support children.");
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

    void Control::setColor(glm::vec3 color)
    {
        m_color.r = color.r;
        m_color.g = color.g;
        m_color.b = color.b;
        setValidationFlags(Validation::Color);
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

    Rect<float> Control::getInteractionBounds() const
    {
        return getAbsoluteBounds();
    }

    void Control::onMouseMoved(float, float)
    {
    }

    void Control::onMouseEntered(float, float)
    {
    }

    void Control::onMouseExited(float, float)
    {
    }

    void Control::onMousePressed(float, float)
    {
    }

    void Control::onMouseReleased(float, float)
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

    void Control::visit(std::function<void(Control*)> func)
    {
        func(this);
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

    glm::vec2 Control::getAnchorOffset() const
    {
        const glm::vec2 parentSize = getParentAbsoluteSize();
        const glm::vec2 padding    = getParentPadding();

        switch (m_anchor)
        {
        default:
        case Anchor::TopLeft:      return padding;
        case Anchor::TopCenter:    return glm::vec2(parentSize.x / 2.0f, padding.y);
        case Anchor::TopRight:     return glm::vec2(parentSize.x - padding.x, padding.y);
        case Anchor::CenterLeft:   return glm::vec2(padding.x, parentSize.y / 2.0f);
        case Anchor::Center:       return parentSize / 2.0f;
        case Anchor::CenterRight:  return glm::vec2(parentSize.x - padding.x, parentSize.y / 2.0f);
        case Anchor::BottomLeft:   return glm::vec2(padding.x, parentSize.y - padding.y);
        case Anchor::BottomCenter: return glm::vec2(parentSize.x / 2.0f, parentSize.y - padding.y);
        case Anchor::BottomRight:  return parentSize - padding;
        }
    }

    glm::vec2 Control::getOriginOffset() const
    {
        const glm::vec2 size = -getSize();

        switch (m_origin)
        {
        default:
        case Origin::TopLeft:      return glm::vec2(0.0f);
        case Origin::TopCenter:    return glm::vec2(size.x / 2.0f, 0.0f);
        case Origin::TopRight:     return glm::vec2(size.x, 0.0f);
        case Origin::CenterLeft:   return glm::vec2(0.0f, size.y / 2.0f);
        case Origin::Center:       return size / 2.0f;
        case Origin::CenterRight:  return glm::vec2(size.x, size.y / 2.0f);
        case Origin::BottomLeft:   return glm::vec2(0.0f, size.y);
        case Origin::BottomCenter: return glm::vec2(size.x / 2.0f, size.y);
        case Origin::BottomRight:  return size;
        }
    }

    glm::vec2 Control::getOffsetDirection() const
    {
        switch (m_anchor)
        {
        default:
        case Anchor::TopLeft:      return glm::vec2(1.0f);
        case Anchor::TopCenter:    return glm::vec2(1.0f);
        case Anchor::TopRight:     return glm::vec2(-1.0f, 1.0f);
        case Anchor::CenterLeft:   return glm::vec2(1.0f);
        case Anchor::Center:       return glm::vec2(1.0f);
        case Anchor::CenterRight:  return glm::vec2(-1.0f, 1.0f);
        case Anchor::BottomLeft:   return glm::vec2(1.0f, -1.0f);
        case Anchor::BottomCenter: return glm::vec2(1.0f, -1.0f);
        case Anchor::BottomRight:  return glm::vec2(-1.0f);
        }
    }

    glm::vec2 Control::getAbsolutePosition() const
    {
        const glm::vec2 parentAbsPos  = getParentAbsolutePosition();
        const glm::vec2 dir = getOffsetDirection();

        const glm::vec2 anchorOffset = getAnchorOffset();
        const glm::vec2 originOffset = getOriginOffset();

        return parentAbsPos + anchorOffset + originOffset + dir * m_position;
    }

    float Control::getAbsoluteDepth() const
    {
        return getParentAbsoluteDepth() + m_depthOffset;
    }
}