#include <Crisp/Animation/Animator.hpp>

#include <algorithm>

namespace crisp
{
Animator::Animator()
    : m_clearAllAnimations(false)
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
        m_animationLifetimes.clear();
        m_clearAllAnimations = false;
    }

    // Remove all animations scheduled for removal
    if (!m_removedAnimations.empty())
    {
        m_activeAnimations.erase(std::remove_if(m_activeAnimations.begin(), m_activeAnimations.end(),
                                     [this](const std::shared_ptr<Animation>& anim)
                                     {
                                         return m_removedAnimations.contains(anim);
                                     }),
            m_activeAnimations.end());

        m_pendingAnimations.erase(std::remove_if(m_pendingAnimations.begin(), m_pendingAnimations.end(),
                                      [this](const std::shared_ptr<Animation>& anim)
                                      {
                                          return m_removedAnimations.contains(anim);
                                      }),
            m_pendingAnimations.end());

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
        m_activeAnimations.erase(
            std::remove_if(
                m_activeAnimations.begin(),
                m_activeAnimations.end(),
                [](const std::shared_ptr<Animation>& anim)
                {
                    return anim->isFinished();
                }),
            m_activeAnimations.end());
    }
}

void Animator::add(std::shared_ptr<Animation> anim)
{
    m_pendingAnimations.emplace_back(anim);
    anim->setActive(true);
}

void Animator::add(std::shared_ptr<Animation> animation, void* targetObject)
{
    m_pendingAnimations.emplace_back(animation);
    m_animationLifetimes[targetObject].emplace_back(animation);
    animation->setActive(true);
}

void Animator::remove(std::shared_ptr<Animation> anim)
{
    m_removedAnimations.insert(anim);
}

void Animator::clearAllAnimations()
{
    m_clearAllAnimations = true;
}

void Animator::clearObjectAnimations(void* targetObject)
{
    auto iter = m_animationLifetimes.find(targetObject);
    if (iter == m_animationLifetimes.end())
    {
        return;
    }

    for (auto& anim : iter->second)
    {
        m_removedAnimations.insert(anim);
    }

    m_animationLifetimes.erase(iter);
}
} // namespace crisp
