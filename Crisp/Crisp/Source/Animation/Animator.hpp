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
        void clearAllAnimations();

    private:
        std::list<std::shared_ptr<Animation>> m_activeAnimations;
        std::list<std::shared_ptr<Animation>> m_pendingAnimations;

        bool m_hasFinishedAnimations;
        bool m_clearAllAnimations;
    };
}