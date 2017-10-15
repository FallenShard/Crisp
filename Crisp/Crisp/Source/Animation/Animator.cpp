#include "Animator.hpp"

#include <iostream>

namespace crisp
{
    Animator::Animator()
        : m_clearAllAnimations(false)
    {
    }

    Animator::~Animator()
    {
        std::cout << "Deleting animator!" << '\n';
        clearAllAnimations();
    }

    void Animator::update(double dt)
    {
        // Clear all animations if requested
        if (m_clearAllAnimations)
        {
            m_activeAnimations.clear();
            m_clearAllAnimations = false;
        }

        // Remove all animations scheduled for removal
        if (!m_removedAnimations.empty())
        {
            for (auto& removedAnim : m_removedAnimations)
                m_activeAnimations.remove(removedAnim);
        }

        // Add in any new animations
        if (!m_pendingAnimations.empty())
            m_activeAnimations.splice(m_activeAnimations.end(), m_pendingAnimations);

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
            m_activeAnimations.remove_if([](const std::shared_ptr<crisp::Animation>& anim)
            {
                return anim->isFinished();
            });
            hasFinishedAnimations = false;
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