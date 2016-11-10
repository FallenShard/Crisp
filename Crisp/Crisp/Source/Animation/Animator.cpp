#include "Animator.hpp"

namespace crisp
{
    Animator::Animator()
        : m_clearAllAnimations(false)
        , m_hasFinishedAnimations(false)
    {
    }

    Animator::~Animator()
    {
        clearAllAnimations();
    }

    void Animator::update(double dt)
    {
        if (!m_pendingAnimations.empty())
            m_activeAnimations.splice(m_activeAnimations.end(), m_pendingAnimations);

        m_hasFinishedAnimations = false;
        for (auto& item : m_activeAnimations)
        {
            item->update(dt);
            if (item->isFinished())
                m_hasFinishedAnimations = true;
        }

        if (m_hasFinishedAnimations)
        {
            m_activeAnimations.remove_if([](const std::shared_ptr<crisp::Animation>& anim)
            {
                return anim->isFinished();
            });
            m_hasFinishedAnimations = false;
        }

        if (m_clearAllAnimations)
        {
            m_activeAnimations.clear();
            m_clearAllAnimations = false;
        }
    }

    void Animator::add(std::shared_ptr<Animation> anim)
    {
        m_pendingAnimations.emplace_back(anim);
    }

    void Animator::clearAllAnimations()
    {
        m_clearAllAnimations = true;
    }
}