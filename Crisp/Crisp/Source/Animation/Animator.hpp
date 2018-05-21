#pragma once

#include <list>
#include <memory>

#include "Animation.hpp"

namespace crisp
{
    class Animator
    {
    public:
        Animator();
        ~Animator();

        void update(double dt);

        void add(std::shared_ptr<Animation> animation);
        void remove(std::shared_ptr<Animation> animation);
        void clearAllAnimations();

    private:
        std::vector<std::shared_ptr<Animation>> m_activeAnimations;
        std::vector<std::shared_ptr<Animation>> m_pendingAnimations;
        std::vector<std::shared_ptr<Animation>> m_removedAnimations;

        bool m_clearAllAnimations;
    };
}