#pragma once

#include <memory>

#include <vulkan/vulkan.h>

#include "Core/Keyboard.hpp"

namespace crisp
{
    class FluidSimulation
    {
    public:
        virtual ~FluidSimulation() {}

        virtual void update(float dt) = 0;

        virtual void dispatchCompute(VkCommandBuffer cmdBuffer, uint32_t currentFrameIdx) const = 0;

        virtual void onKeyPressed(Key key, int modifier) = 0;
        virtual void setGravityX(float value) = 0;
        virtual void setGravityY(float value) = 0;
        virtual void setGravityZ(float value) = 0;
        virtual void setViscosity(float value) = 0;
        virtual void setSurfaceTension(float value) = 0;
        virtual void reset() = 0;

        virtual float getParticleRadius() const = 0;

        virtual void drawGeometry(VkCommandBuffer cmdBuffer) const = 0;
    };
}