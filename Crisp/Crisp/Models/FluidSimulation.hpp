#pragma once

#include <memory>

#include <Crisp/Core/Keyboard.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/VulkanBuffer.hpp>


namespace crisp {
class FluidSimulation {
public:
    virtual ~FluidSimulation() {}

    virtual void update(float dt) = 0;

    virtual void onKeyPressed(Key key, int modifier) = 0;
    virtual void setGravityX(float value) = 0;
    virtual void setGravityY(float value) = 0;
    virtual void setGravityZ(float value) = 0;
    virtual void setViscosity(float value) = 0;
    virtual void setSurfaceTension(float value) = 0;
    virtual void reset() = 0;

    virtual float getParticleRadius() const = 0;

    virtual VulkanBuffer* getVertexBuffer(std::string_view key) const = 0;

    virtual uint32_t getParticleCount() const = 0;

    virtual uint32_t getCurrentSection() const = 0;
};
} // namespace crisp