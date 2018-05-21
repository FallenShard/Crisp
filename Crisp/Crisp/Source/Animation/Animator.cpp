#include "Animator.hpp"

#include <algorithm>

namespace crisp
{
    Animator::Animator()
        : m_clearAllAnimations(false)
    {
    }

    Animator::~Animator()
    {
    }

    void Animator::update(double dt)
    {
        // Clear all animations if requested
        if (m_clearAllAnimations)
        {
            m_pendingAnimations.clear();
            m_activeAnimations.clear();
            m_removedAnimations.clear();
            m_clearAllAnimations = false;
        }

        // Remove all animations scheduled for removal
        if (!m_removedAnimations.empty())
        {
            m_activeAnimations.erase(std::remove_if(m_activeAnimations.begin(), m_activeAnimations.end(), [this](const std::shared_ptr<Animation>& anim)
            {
                for (auto& removedAnim : m_removedAnimations)
                    if (anim.get() == removedAnim.get())
                        return true;
                return false;
            }), m_activeAnimations.end());
            m_removedAnimations.clear();
        }

        // Add in any new animations
        if (!m_pendingAnimations.empty())
        {
            m_activeAnimations.insert(m_activeAnimations.end(), m_pendingAnimations.begin(), m_pendingAnimations.end());
            m_pendingAnimations.clear();
        }

        // Advance all animations by a step
        bool hasFinishedAnimations = false;
        for (auto& animation : m_activeAnimations)
        {
            animation->update(dt);
            if (animation->isFinished())
            {
                animation->setActive(false);
                hasFinishedAnimations = true;
            }
        }

        // Remove all finished animations
        if (hasFinishedAnimations)
        {
            m_activeAnimations.erase(std::remove_if(m_activeAnimations.begin(), m_activeAnimations.end(), [this](const std::shared_ptr<Animation>& anim)
            {
                return anim->isFinished();
            }), m_activeAnimations.end());
        }
    }

    void Animator::add(std::shared_ptr<Animation> anim)
    {
        m_pendingAnimations.emplace_back(anim);
        anim->setActive(true);
    }

    void Animator::remove(std::shared_ptr<Animation> anim)
    {
        m_removedAnimations.emplace_back(anim);
    }

    void Animator::clearAllAnimations()
    {
        m_clearAllAnimations = true;
    }
}