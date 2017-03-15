#pragma once

#include <memory>
#include <unordered_map>
#include <set>

#include <vulkan/vulkan.h>

#include "Math/Headers.hpp"
#include "IO/FontLoader.hpp"

#include "Vulkan/VulkanRenderer.hpp"

namespace crisp
{
    class VulkanRenderer;
    class VulkanPipeline;
    class GuiColorQuadPipeline;
    class GuiTextPipeline;
    class GuiTexQuadPipeline;
    class FullScreenQuadPipeline;

    class GuiRenderPass;

    struct Font;

    namespace gui
    {
        class Control;
        class Button;
        class CheckBox;
        class Label;
        class Panel;

        enum ColorPalette
        {
            DarkGray,
            Green,
            LightGreen,
            DarkGreen,
            LightGray,
            White,
            MediumGray,
            Blue,

            Gray20,
            Gray30,
            Gray40,

            ColorCount
        };

        class RenderSystem
        {
        public:
            static constexpr uint32_t GuiRenderPassId = 64;

            RenderSystem(VulkanRenderer* renderer);
            ~RenderSystem();

            const glm::mat4& getProjectionMatrix() const;

            unsigned int registerTransformResource();
            void updateTransformResource(unsigned int transformId, const glm::mat4& M);
            void unregisterTransformResource(unsigned int transformId);

            unsigned int registerColorResource();
            void updateColorResource(unsigned int colorId, const glm::vec4& color);
            void unregisterColorResource(unsigned int colorId);

            unsigned int registerTexCoordResource();
            void updateTexCoordResource(unsigned int resourceId, const glm::vec2& min, const glm::vec2& max);
            void unregisterTexCoordResource(unsigned int resourceId);

            unsigned int registerTextResource(std::string text, unsigned int fontId);
            glm::vec2 updateTextResource(unsigned int textResId, const std::string& text, unsigned int fontId);
            void unregisterTextResource(unsigned int textResId);
            glm::vec2 queryTextExtent(std::string text, unsigned int fontId);

            void draw(const CheckBox& checkBox);

            void drawQuad(unsigned int transformId, uint32_t colorResourceId, float depth) const;
            void drawText(unsigned int textRenderResourceId, unsigned int transformResourceId, uint32_t colorResourceId, float depth);

            void submitDrawRequests();

            void resize(int width, int height);


            DeviceMemoryMetrics getDeviceMemoryUsage();
            glm::vec2 getScreenSize() const;

            uint32_t getFont(std::string name, uint32_t pixelSize);

        private:
            void createPipelines();
            void initGeometryBuffers();
            void initTransformsResources();
            void initColorResources();
            void initGuiRenderTargetResources();

            void loadTextures();

            void updateTransformUniformBuffer(uint32_t frameId);
            void updateColorBuffer(uint32_t frameId);

            VulkanRenderer* m_renderer;
            VulkanDevice*   m_device;

            static constexpr float DepthLayers = 32.0f;
            glm::mat4 m_P;

            std::unique_ptr<GuiRenderPass> m_guiPass;
            std::unique_ptr<GuiColorQuadPipeline>   m_colorQuadPipeline;
            std::unique_ptr<GuiTextPipeline>        m_textPipeline;
            std::unique_ptr<GuiTexQuadPipeline>     m_texQuadPipeline;
            std::unique_ptr<FullScreenQuadPipeline> m_fsQuadPipeline;

            // Geometry
            struct GeometryData
            {
                VkBuffer vertexBuffer;
                VkDeviceSize vertexBufferOffset;
                VkBuffer indexBuffer;
                VkDeviceSize indexBufferOffset;
                uint32_t indexCount;
            };

            // [0..1] quad geometry
            GeometryData m_quadGeometry;

            // MVP Transforms resources
            static constexpr unsigned int UniformBufferGranularity = 256;
            static constexpr unsigned int MatrixSize = sizeof(glm::mat4);
            static constexpr unsigned int MatricesPerGranularity = UniformBufferGranularity / MatrixSize;
            struct UniformBufferFrameResource
            {
                VkBuffer        uniformBuffer;
                VkDeviceSize    bufferSize;
                VkDescriptorSet descSet;
                bool            isUpdated;

                void createBufferAndUpdateSet(VulkanDevice* device, VkDeviceSize size, uint32_t binding);
            };
            UniformBufferFrameResource m_transformsResources[VulkanRenderer::NumVirtualFrames];
            VkBuffer                   m_transformsStagingBuffer;
            VkDeviceSize               m_transformsStagingBufferSize;
            std::set<unsigned int>     m_transformsResourceIdPool;
            unsigned int               m_numRegisteredTransformResources;

            // Color resources
            static constexpr unsigned int ColorSize = sizeof(glm::vec4);
            static constexpr unsigned int ColorsPerGranularity = UniformBufferGranularity / ColorSize;
            UniformBufferFrameResource m_colorsResources[VulkanRenderer::NumVirtualFrames];
            VkBuffer                   m_colorsStagingBuffer;
            VkDeviceSize               m_colorsStagingBufferSize;
            std::set<unsigned int>     m_colorsResourceIdPool;
            unsigned int               m_numRegisteredColorResources;

            // Sampler
            VkSampler m_linearClampSampler;
            VkSampler m_nnClampSampler;
            
            // Gui render target
            VkDescriptorSet m_fsQuadDescSet;
            VkImageView     m_guiImageView;

            //VkImage   m_checkBoxImage;
            //VkImageView m_checkBoxImageView;

            // Font resources
            struct FontTexture
            {
                std::unique_ptr<Font> font;
                VkImage               image;
                VkImageView           imageView;
                VkDescriptorSet       descSet;
            };
            std::vector<std::unique_ptr<FontTexture>> m_fonts;
            FontLoader m_fontLoader;
            
            std::vector<VkDescriptorSet> m_texQuadDescSets;

            // Text Resources
            struct TextGeometryResource
            {
                static constexpr uint32_t NumInitialAllocatedCharacters = 32;
                GeometryData geomData;
                VkBuffer stagingVertexBuffer;
                VkBuffer stagingIndexBuffer;

                uint32_t allocatedVertexCount;
                uint32_t vertexCount;
                uint32_t allocatedFaceCount;
                uint32_t faceCount;

                unsigned char updatedBufferIndex;
                bool isUpdatedOnDevice;

                glm::vec2 extent;

                std::vector<VkDescriptorSet> descSets;

                void updateStagingBuffer(std::string text, Font* font, VulkanRenderer* renderer);
            };
            static constexpr uint32_t TextResourceIncrement = 10;
            std::vector<std::unique_ptr<TextGeometryResource>> m_textResources;
            std::set<unsigned int> m_textResourceIdPool;

            /*struct TexCoordResource
            {
                GeometryData geom;
                VkBuffer stagingVertexBuffer;

                unsigned char updatedBufferIndex;
                bool needsUpdateToDevice;

                void generate(const glm::vec2& min, const glm::vec2& max, VulkanRenderer* renderer);
            };

            std::vector<std::unique_ptr<TexCoordResource>> m_texCoordResources;
            std::set<unsigned int> m_freeTexCoordResourceIds;*/

            struct GuiDrawCommand
            {
                const VulkanPipeline* pipeline;
                const VkDescriptorSet* descriptorSets;
                uint8_t firstSet;
                uint8_t descSetCount;
                
                const GeometryData* geom;
                
                uint16_t transformId;
                uint16_t colorId;
                float depth;

                GuiDrawCommand() {}
                GuiDrawCommand(VulkanPipeline* vp, const VkDescriptorSet* dSets, uint8_t first, uint8_t setCount,
                    const GeometryData* gd, uint16_t tid, uint16_t cid, float d)
                    : pipeline(vp), descriptorSets(dSets), firstSet(first), descSetCount(setCount)
                    , geom(gd), transformId(tid), colorId(cid), depth(d) {}
            };

            mutable std::vector<GuiDrawCommand> m_drawCommands;

            
        };
    }
}