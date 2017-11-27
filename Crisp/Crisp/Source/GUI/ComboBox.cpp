#include "GUI/ComboBox.hpp"

#include "Form.hpp"
#include "Label.hpp"
#include "Panel.hpp"
#include "ComboBoxItem.hpp"

#include <iostream>

#include <CrispCore/ConsoleUtils.hpp>

namespace crisp::gui
{
    ComboBox::ComboBox(Form* parentForm, std::vector<std::string> elements)
        : Control(parentForm)
        , m_state(State::Idle)
        , m_stateColors(static_cast<std::size_t>(State::Count))
        , m_label(std::make_unique<Label>(parentForm, "Selected Item"))
        , m_drawComponent(parentForm->getRenderSystem())
        , m_itemsPanel(std::make_unique<ControlGroup>(parentForm))
        , m_showPanel(false)
    {
        setSizeHint({100.0f, 30.0f});

        glm::vec3 color(0.5f, 0.3f, 0.3f);
        m_color        = glm::vec4(color, m_opacity);
        m_stateColors[static_cast<std::size_t>(State::Idle)].background    = color;
        m_stateColors[static_cast<std::size_t>(State::Hover)].background   = glm::clamp(color * 1.25f, glm::vec3(0.0f), glm::vec3(1.0f));
        m_stateColors[static_cast<std::size_t>(State::Pressed)].background = color * 0.75f;
        setValidationFlags(Validation::Color);

        m_M = glm::translate(glm::vec3(m_position, m_depthOffset)) * glm::scale(glm::vec3(m_sizeHint, 1.0f));
        m_color = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);

        m_label->setParent(this);
        m_label->setAnchor(Anchor::Center);

        m_itemsPanel->setParent(this);
        m_itemsPanel->setId("comboBoxItems");
        m_itemsPanel->setPosition({ 0.0f, 30.0f });
        m_itemsPanel->setSizingPolicy(SizingPolicy::Fixed, SizingPolicy::WrapContent);
        m_itemsPanel->setSizeHint({ 100.0f, 0.0f });
        m_itemsPanel->setAnchor(Anchor::TopLeft);

        std::vector<std::string> labels = { "SSAO", "HBAO+", "SSDO" };
        for (int i = 0; i < 3; i++)
        {
            auto item = std::make_unique<ComboBoxItem>(parentForm, labels[i]);
            item->setId("Item " + std::to_string(i));
            item->setPosition({ 0.0f, i * 20.0f });
            item->setSizeHint({ 100.0f, 20.0f });
            item->clicked += [this, item = item.get()]()
            {
                itemSelected(item->getText());
                m_label->setText(item->getText());
                setValidationFlags(Validation::Geometry);
            };
            m_itemsPanel->addControl(std::move(item));
        }
    }

    ComboBox::~ComboBox()
    {
    }

    Rect<float> ComboBox::getInteractionBounds() const
    {
        if (m_showPanel)
            return getAbsoluteBounds().merge(m_itemsPanel->getAbsoluteBounds());
        else
            return getAbsoluteBounds();
    }

    void ComboBox::onMouseEntered(float x, float y)
    {
        if (m_showPanel && m_itemsPanel->getInteractionBounds().contains(x, y))
        {
            m_itemsPanel->onMouseEntered(x, y);
            setValidationFlags(Validation::Color);
        }
    }

    void ComboBox::onMouseMoved(float x, float y)
    {
        if (m_showPanel)
        {
            m_itemsPanel->onMouseMoved(x, y);
            setValidationFlags(Validation::Color);
        }
    }

    void ComboBox::onMouseExited(float x, float y)
    {
        if (m_showPanel)
        {
            m_itemsPanel->onMouseExited(x, y);
            setValidationFlags(Validation::Color);
        }
    }

    void ComboBox::onMousePressed(float x, float y)
    {
        if (m_showPanel)
        {
            m_itemsPanel->onMousePressed(x, y);
            setValidationFlags(Validation::Color);
        }
    }

    void ComboBox::onMouseReleased(float x, float y)
    {
        if (getInteractionBounds().contains(x, y))
        {
            if (m_showPanel)
            {
                m_itemsPanel->onMouseReleased(x, y);
                m_showPanel = false;
            }
            else
            {
                m_showPanel = true;
                m_form->setFocusedControl(this);
            }
        }
        else
        {
            m_showPanel = false;
            m_form->setFocusedControl(nullptr);
        }
    }

    bool ComboBox::needsValidation()
    {
        if (m_validationFlags != Validation::None)
            return true;

        if (m_showPanel)
            return m_itemsPanel->needsValidation();
        else
            return false;
    }

    void ComboBox::validate()
    {
        if (m_validationFlags & Validation::Geometry)
        {
            auto absPos = getAbsolutePosition();
            auto absDepth = getAbsoluteDepth();
            auto absSize = getSize();
            m_itemsPanel->setPosition({ 0.0f, absSize.y });

            m_M = glm::translate(glm::vec3(absPos, absDepth)) * glm::scale(glm::vec3(absSize, 1.0f));
            m_drawComponent.update(m_M);
        }

        if (m_validationFlags & Validation::Color)
        {
            m_color.a = getParentAbsoluteOpacity() * m_opacity;
            m_drawComponent.update(m_color);
        }

        m_label->setValidationFlags(m_validationFlags);
        m_label->validate();
        m_label->clearValidationFlags();

        m_itemsPanel->setValidationFlags(m_validationFlags);
        m_itemsPanel->validate();
        m_itemsPanel->clearValidationFlags();
    }

    void ComboBox::draw(const RenderSystem& renderSystem) const
    {
        m_drawComponent.draw(m_M[3][2]);
        m_label->draw(renderSystem);

        if (m_showPanel)
            m_itemsPanel->draw(renderSystem);
    }

    void ComboBox::setItems(const std::vector<std::string>& items)
    {
        m_itemsPanel->clearControls();
        for (int i = 0; i < items.size(); i++)
        {
            auto item = std::make_unique<ComboBoxItem>(m_form, items[i]);
            item->setId("Item " + std::to_string(i));
            item->setPosition({ 0.0f, i * 20.0f });
            item->setSizeHint({ 100.0f, 20.0f });
            item->clicked += [this, item = item.get()]()
            {
                itemSelected(item->getText());
                m_label->setText(item->getText());
                setValidationFlags(Validation::Geometry);
            };
            m_itemsPanel->addControl(std::move(item));
        }

        m_label->setText(items[0]);
    }

    void ComboBox::setState(State state)
    {
    }
}