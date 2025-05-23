#pragma once

#include <Crisp/Animation/Animation.hpp>

#include <Crisp/Core/HashMap.hpp>

#include <memory>
#include <vector>

namespace crisp {
class Animator {
public:
    Animator();
    ~Animator() = default;

    Animator(const Animator& animator) = delete;
    Animator(Animator&& animator) = default;
    Animator& operator=(const Animator& animator) = delete;
    Animator& operator=(Animator&& animator) = default;

    void update(double dt);

    void add(const std::shared_ptr<Animation>& animation);
    void add(const std::shared_ptr<Animation>& animation, void* targetObject);
    void remove(const std::shared_ptr<Animation>& animation);
    void clearAllAnimations();
    void clearObjectAnimations(void* targetObject);

private:
    // The list of animations that are currently actively updated
    std::vector<std::shared_ptr<Animation>> m_activeAnimations;

    // The list of animations that will enter the list of active animations
    // The list is separate to allow addition of animations inside an animation handler
    // This avoids invalidation of the m_activeAnimations container iterator
    std::vector<std::shared_ptr<Animation>> m_pendingAnimations;

    // The set of animations slated to be removed from active animations list
    HashSet<std::shared_ptr<Animation>> m_removedAnimations;

    // Ties the lifetime of a group of animations to a particular object pointer
    HashMap<void*, std::vector<std::shared_ptr<Animation>>> m_animationLifetimes;

    bool m_clearAllAnimations;
};
} // namespace crisp