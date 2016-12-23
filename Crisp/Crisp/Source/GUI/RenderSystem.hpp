#pragma once

#include <memory>
#include <unordered_map>
#include <set>

#include <vulkan/vulkan.h>

#include "Math/Headers.hpp"

#include "vulkan/DrawItem.hpp"

namespace crisp
{
    class VulkanRenderer;
    class VulkanPipeline;
    class GuiColorQuadPipeline;
    class GuiTextPipeline;
    class GuiTexQuadPipeline;

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

            Gray20,
            Gray30,
            Gray40,

            ColorCount
        };

        class RenderSystem
        {
        public:
            RenderSystem(VulkanRenderer* renderer);
            ~RenderSystem();

            const glm::mat4& getProjectionMatrix() const;

            unsigned int registerTransformResource();
            void updateTransformResource(unsigned int transformId, const glm::mat4& M);
            void unregisterTransformResource(unsigned int transformId);

            unsigned int registerTexCoordResource();
            void updateTexCoordResource(unsigned int resourceId, const glm::vec2& min, const glm::vec2& max);
            void unregisterTexCoordResource(unsigned int resourceId);

            unsigned int registerTextResource();
            glm::vec2 updateTextResource(unsigned int textResId, const std::string& text);
            void unregisterTextResource(unsigned int textResId);
            glm::vec2 queryTextExtent(std::string text);

            void draw(const Panel& panel);
            void draw(const Button& button);
            void draw(const CheckBox& checkBox);
            void draw(const Label& label);

            void submitDrawRequests();

            void resize(int width, int height);

            void buildResourceBuffers();

            std::pair<uint64_t, uint64_t> getDeviceMemoryUsage();

        private:
            void loadFonts();
            void loadTextures();
            void createPipelines();
            void initColorPaletteBuffer();
            void initGeometryBuffers();
            void updateDescriptorSets();

            VulkanRenderer* m_renderer;
            glm::mat4 m_P;

            std::unique_ptr<GuiRenderPass> m_guiPass;

            VkImage   m_checkBoxImage;
            VkImageView m_checkBoxImageView;
            VkSampler m_checkBoxSampler;

            // Font resources
            struct FontTexture
            {
                std::unique_ptr<Font> font;
                VkImage image;
                VkImageView imageView;
            };
            std::unordered_map<std::string, std::unique_ptr<FontTexture>> m_fonts;
            FontTexture* m_labelFont;
            VkSampler m_fontSampler;

            // Pipelines
            std::unique_ptr<GuiColorQuadPipeline> m_colorQuadPipeline;
            std::unique_ptr<GuiTextPipeline> m_textPipeline;
            std::unique_ptr<GuiTexQuadPipeline> m_texQuadPipeline;

            VkDescriptorSet m_transformSet;
            std::vector<VkDescriptorSet> m_colorQuadDescSets;
            std::vector<VkDescriptorSet> m_textDescSets;
            std::vector<VkDescriptorSet> m_texQuadDescSets;

            // Colors
            VkBuffer m_colorPaletteBuffer;

            // MVP Resources
            VkBuffer m_transBuffer;
            VkBuffer m_transStagingBuffer;
            VkDeviceSize m_transBufferSize;
            uint32_t m_updatedTransBufferIndex;

            static constexpr unsigned int UniformBufferGranularity = 256;
            static constexpr unsigned int MatrixSize = sizeof(glm::mat4);
            static constexpr unsigned int MatricesPerUniformBuffer = UniformBufferGranularity / MatrixSize;
            struct GuiTransform
            {
                glm::mat4 MVP[MatricesPerUniformBuffer];
            };
            std::vector<GuiTransform> m_mvpTransforms;
            std::set<unsigned int> m_freeTransformIds;

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

            // Text Resources
            struct TextVertexResource
            {
                GeometryData geomData;
                VkBuffer stagingVertexBuffer;
                VkBuffer stagingIndexBuffer;

                uint32_t allocatedVertexCount;
                uint32_t vertexCount;
                uint32_t allocatedFaceCount;
                uint32_t faceCount;

                unsigned char updatedBufferIndex;
                bool needsUpdateToDevice;

                std::string text;
                glm::vec2 extent;

                void updateStagingBuffer(Font* font, VulkanRenderer* renderer);
            };
            std::vector<std::unique_ptr<TextVertexResource>> m_textResources;
            std::set<unsigned int> m_freeTextResourceIds;

            struct TexCoordResource
            {
                GeometryData geom;
                VkBuffer stagingVertexBuffer;

                unsigned char updatedBufferIndex;
                bool needsUpdateToDevice;

                void generate(const glm::vec2& min, const glm::vec2& max, VulkanRenderer* renderer);
            };

            std::vector<std::unique_ptr<TexCoordResource>> m_texCoordResources;
            std::set<unsigned int> m_freeTexCoordResourceIds;

            struct GuiDrawCommand
            {
                VulkanPipeline* pipeline;
                VkDescriptorSet* descriptorSets;
                uint8_t firstSet;
                uint8_t descSetCount;
                
                GeometryData* geom;
                
                uint16_t transformId;
                ColorPalette colorIndex;
                float depth;

                GuiDrawCommand() {}
                GuiDrawCommand(VulkanPipeline* vp, VkDescriptorSet* dSets, uint8_t first, uint8_t setCount,
                    GeometryData* gd, uint32_t tid, ColorPalette cidx, float d)
                    : pipeline(vp), descriptorSets(dSets), firstSet(first), descSetCount(setCount)
                    , geom(gd), transformId(tid), colorIndex(cidx), depth(d) {}
            };

            std::vector<GuiDrawCommand> m_drawCommands;

            VkImageView m_guiImageView;
        };
    }
}