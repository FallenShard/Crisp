#pragma once

#include <memory>
#include <set>
#include <unordered_map>

#include <Crisp/Vulkan/VulkanHeader.hpp>
#include <Crisp/Vulkan/VulkanSampler.hpp>

#include <Crisp/IO/FontLoader.hpp>
#include <Crisp/Math/Headers.hpp>
#include <Crisp/Math/Rect.hpp>

#include <Crisp/Renderer/IndexBuffer.hpp>
#include <Crisp/Renderer/VertexBuffer.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Renderer/Material.hpp>

#include <Crisp/Renderer/RenderTargetCache.hpp>

namespace crisp
{
class Renderer;
class VulkanPipeline;
class VulkanImageView;
class VulkanDevice;

class IndexBuffer;
class Texture;

class VulkanRenderPass;

struct Font;

namespace gui
{
class Control;
class Button;
class CheckBox;
class Label;
class Panel;

class DynamicUniformBufferResource;

class RenderSystem
{
public:
    RenderSystem(Renderer* renderer);
    ~RenderSystem();

    const glm::mat4& getProjectionMatrix() const;

    unsigned int registerTransformResource();
    void updateTransformResource(unsigned int transformId, const glm::mat4& M);
    void unregisterTransformResource(unsigned int transformId);

    unsigned int registerColorResource();
    void updateColorResource(unsigned int colorId, const glm::vec4& color);
    void unregisterColorResource(unsigned int colorId);

    unsigned int registerTexCoordResource();
    void updateTexCoordResource(unsigned int resourceId, const glm::vec4& tcTransform);
    void unregisterTexCoordResource(unsigned int resourceId);

    unsigned int registerTextResource(std::string text, unsigned int fontId);
    glm::vec2 updateTextResource(unsigned int textResId, const std::string& text, unsigned int fontId);
    void unregisterTextResource(unsigned int textResId);
    glm::vec2 queryTextExtent(std::string text, unsigned int fontId) const;

    void drawQuad(unsigned int transformId, uint32_t colorResourceId, float depth) const;
    void drawTexture(unsigned int transformId, unsigned int colorId, unsigned int texCoordId, float depth) const;
    void drawText(
        unsigned int textRenderResourceId,
        unsigned int transformResourceId,
        uint32_t colorResourceId,
        float depth) const;
    void drawDebugRect(Rect<float> rect, glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)) const;

    void submitDrawCommands();

    void resize(int width, int height);

    const Renderer& getRenderer() const;
    glm::vec2 getScreenSize() const;

    uint32_t getFont(std::string name, uint32_t pixelSize);

private:
    void createPipelines();
    void loadTextureAtlas();
    void initGeometryBuffers();
    void initGuiRenderTargetResources();
    void updateFullScreenMaterial();

    Renderer* m_renderer;

    glm::mat4 m_P;

    std::unique_ptr<VulkanRenderPass> m_guiPass;
    std::unique_ptr<VulkanPipeline> m_colorQuadPipeline;
    std::unique_ptr<VulkanPipeline> m_textPipeline;
    std::unique_ptr<VulkanPipeline> m_texQuadPipeline;
    std::unique_ptr<VulkanPipeline> m_debugRectPipeline;
    std::unique_ptr<VulkanPipeline> m_fsQuadPipeline;

    // canonical square [0,1]x[0,1] quad geometry
    std::unique_ptr<Geometry> m_quadGeometry;
    std::unique_ptr<Geometry> m_lineLoopGeometry;

    std::unique_ptr<DynamicUniformBufferResource> m_transforms;
    std::unique_ptr<DynamicUniformBufferResource> m_colors;
    std::unique_ptr<DynamicUniformBufferResource> m_tcTransforms;

    std::unique_ptr<Texture> m_guiAtlas;
    std::unique_ptr<VulkanImageView> m_guiAtlasView;

    // Sampler
    std::unique_ptr<VulkanSampler> m_linearClampSampler;

    std::unique_ptr<Material> m_fsMaterial;

    // Font resources
    struct FontTexture
    {
        std::unique_ptr<Font> font;
        std::unique_ptr<Texture> texture;
        std::unique_ptr<VulkanImageView> VulkanImageView;
        VkDescriptorSet descSet;
    };

    std::vector<std::unique_ptr<FontTexture>> m_fonts;
    FontLoader m_fontLoader;

    // Text Resources
    struct TextGeometryResource
    {
        static constexpr uint32_t kNumInitialAllocatedCharacters = 32;

        uint32_t allocatedVertexCount;
        uint32_t vertexCount;
        uint32_t allocatedFaceCount;

        uint32_t firstBinding;
        uint32_t bindingCount;
        std::vector<VkBuffer> buffers;
        std::vector<VkDeviceSize> offsets;
        std::unique_ptr<VertexBuffer> vertexBuffer;
        std::unique_ptr<IndexBuffer> indexBuffer;
        VkDeviceSize indexBufferOffset;
        uint32_t indexCount;

        unsigned char updatedBufferIndex;
        bool isUpdatedOnDevice;

        glm::vec2 extent;

        VkDescriptorSet descSet;

        void updateStagingBuffer(std::string text, const Font& font);
        void drawIndexed(VkCommandBuffer cmdBuffer) const;
    };

    static constexpr uint32_t kTextResourceIncrement = 10;
    std::vector<std::unique_ptr<TextGeometryResource>> m_textResources;
    std::set<unsigned int> m_textResourceIdPool;

    struct GuiDrawCommand
    {
        using Callback = void (RenderSystem::*)(VkCommandBuffer, uint32_t, const GuiDrawCommand&) const;
        Callback drawFuncPtr;
        uint32_t transformId;
        uint32_t colorId;
        uint32_t textId;
        float depth;

        GuiDrawCommand(Callback callback, uint32_t transId, uint32_t colorId, float d)
            : drawFuncPtr(callback)
            , transformId(transId)
            , colorId(colorId)
            , depth(d)
            , textId(0)
        {
        }

        GuiDrawCommand(Callback callback, uint32_t transId, uint32_t colorId, uint32_t textResId, float d)
            : drawFuncPtr(callback)
            , transformId(transId)
            , colorId(colorId)
            , depth(d)
            , textId(textResId)
        {
        }
    };

    void renderQuad(VkCommandBuffer cmdBuffer, uint32_t frameIdx, const GuiDrawCommand& drawCommand) const;
    void renderText(VkCommandBuffer cmdBuffer, uint32_t frameIdx, const GuiDrawCommand& drawCommand) const;
    void renderTexture(VkCommandBuffer cmdBuffer, uint32_t frameIdx, const GuiDrawCommand& drawCommand) const;
    void renderDebugRect(VkCommandBuffer cmdBuffer, const Rect<float>& rect, const glm::vec4& color) const;

    RenderTargetCache m_renderTargetCache;

    mutable std::vector<GuiDrawCommand> m_drawCommands;

    mutable std::vector<Rect<float>> m_debugRects;
    mutable std::vector<glm::vec4> m_rectColors;
};
} // namespace gui
} // namespace crisp