#include "RenderSystem.hpp"
#define NOMINMAX

#include <algorithm>
#include <iostream>

#include <Crisp/IO/FontLoader.hpp>
#include <CrispCore/IO/ImageLoader.hpp>

#include <Crisp/vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanPipeline.hpp>
#include <Crisp/vulkan/VulkanImage.hpp>
#include <Crisp/vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanSampler.hpp>
#include <Crisp/vulkan/VulkanDescriptorSet.hpp>

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/RenderPasses/GuiRenderPass.hpp>
#include <Crisp/Renderer/RenderPasses/DefaultRenderPass.hpp>
#include <Crisp/Renderer/Texture.hpp>

#include <Crisp/GUI/DynamicUniformBufferResource.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace
{
    static constexpr float DepthLayers = 32.0f;

    auto logger = spdlog::stdout_color_mt("RenderSystem");
}

namespace crisp::gui
{
    RenderSystem::RenderSystem(Renderer* renderer)
        : m_renderer(renderer)
        , m_device(renderer->getDevice())
    {
        float width  = static_cast<float>(m_renderer->getSwapChainExtent().width);
        float height = static_cast<float>(m_renderer->getSwapChainExtent().height);
        m_P = glm::ortho(0.0f, width, 0.0f, height, 0.5f, 0.5f + DepthLayers);

        // Create the render pass where all GUI controls will be drawn
        m_guiPass = std::make_unique<GuiRenderPass>(m_renderer);

        // Create pipelines for different types of drawable objects
        createPipelines();
        logger->info("Pipelines initialized");

        // Gui texture atlas
        loadTextureAtlas();
        m_linearClampSampler = std::make_unique<VulkanSampler>(m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        logger->info("Texture initialized");

        // Initialize gui render target rendering resources
        initGuiRenderTargetResources();

        // [0..1] Quad geometries for GUI elements
        initGeometryBuffers();

        // Initialize resources to support dynamic addition of MVP transform resources

        std::array<VkDescriptorSet, RendererConfig::VirtualFrameCount> transformAndColorSets;
        for (uint32_t i = 0; i < RendererConfig::VirtualFrameCount; i++)
            transformAndColorSets[i] = m_colorQuadPipeline->allocateDescriptorSet(0).getHandle();
        m_transforms = std::make_unique<DynamicUniformBufferResource>(m_renderer, transformAndColorSets, static_cast<uint32_t>(sizeof(glm::mat4)), 0);

        // Initialize resources to support dynamic addition of color resources
        m_colors = std::make_unique<DynamicUniformBufferResource>(m_renderer, transformAndColorSets, static_cast<uint32_t>(sizeof(glm::vec4)), 1);

        // Initialize resources to support dynamic addition of textured controls
        std::array<VkDescriptorSet, RendererConfig::VirtualFrameCount> tcSets;
        for (uint32_t i = 0; i < RendererConfig::VirtualFrameCount; i++)
            tcSets[i] = m_texQuadPipeline->allocateDescriptorSet(1).getHandle();

        for (auto& set : tcSets)
        {
            const auto imageInfo = m_guiAtlasView->getDescriptorInfo(m_linearClampSampler.get());

            VkWriteDescriptorSet descWrite = {};
            descWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descWrite.dstSet          = set;
            descWrite.dstBinding      = 0;
            descWrite.dstArrayElement = 0;
            descWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descWrite.descriptorCount = 1;
            descWrite.pImageInfo      = &imageInfo;
            vkUpdateDescriptorSets(m_device->getHandle(), 1, &descWrite, 0, nullptr);
        }
        m_tcTransforms = std::make_unique<DynamicUniformBufferResource>(m_renderer, tcSets, static_cast<uint32_t>(sizeof(glm::vec4)), 1);

        logger->info("Initialized");
    }

    RenderSystem::~RenderSystem()
    {
    }

    const glm::mat4& RenderSystem::getProjectionMatrix() const
    {
        return m_P;
    }

    unsigned int RenderSystem::registerTransformResource()
    {
        return m_transforms->registerResource();
    }

    void RenderSystem::updateTransformResource(unsigned int transformId, const glm::mat4& M)
    {
        m_transforms->updateResource(transformId, glm::value_ptr(m_P * M));
    }

    void RenderSystem::unregisterTransformResource(unsigned int transformId)
    {
        m_transforms->unregisterResource(transformId);
    }

    unsigned int RenderSystem::registerColorResource()
    {
        return m_colors->registerResource();
    }

    void RenderSystem::updateColorResource(unsigned int colorId, const glm::vec4& color)
    {
        m_colors->updateResource(colorId, glm::value_ptr(color));
    }

    void RenderSystem::unregisterColorResource(unsigned int colorId)
    {
        m_colors->unregisterResource(colorId);
    }

    unsigned int RenderSystem::registerTexCoordResource()
    {
        return m_tcTransforms->registerResource();
    }

    void RenderSystem::updateTexCoordResource(unsigned int tcTransId, const glm::vec4& tcTrans)
    {
        m_tcTransforms->updateResource(tcTransId, glm::value_ptr(tcTrans));
    }

    void RenderSystem::unregisterTexCoordResource(unsigned int tcTransId)
    {
        m_tcTransforms->unregisterResource(tcTransId);
    }

    unsigned int RenderSystem::registerTextResource(std::string text, unsigned int fontId)
    {
        if (m_textResourceIdPool.empty())
        {
            for (uint32_t i = 0; i < TextResourceIncrement; ++i) m_textResourceIdPool.insert(static_cast<uint32_t>(m_textResources.size()) + i);
        }

        auto freeTextResourceId = *m_textResourceIdPool.begin(); // Smallest element in a set is at .begin()
        m_textResourceIdPool.erase(freeTextResourceId);

        auto textRes = std::make_unique<TextGeometryResource>();
        textRes->allocatedVertexCount = TextGeometryResource::NumInitialAllocatedCharacters * 4; // 4 Vertices per letter
        textRes->allocatedFaceCount   = TextGeometryResource::NumInitialAllocatedCharacters * 2; // 2 Triangles per letter
        textRes->updatedBufferIndex   = 0;
        textRes->isUpdatedOnDevice    = false;
        textRes->vertexBuffer         = std::make_unique<VertexBuffer>(m_renderer, textRes->allocatedVertexCount * sizeof(glm::vec4), BufferUpdatePolicy::PerFrame);
        textRes->indexBuffer          = std::make_unique<IndexBuffer>(m_renderer, VK_INDEX_TYPE_UINT16, BufferUpdatePolicy::PerFrame, textRes->allocatedFaceCount * sizeof(glm::u16vec3));
        textRes->firstBinding         = 0;
        textRes->bindingCount         = 1;
        textRes->buffers              = { textRes->vertexBuffer->get() };
        textRes->offsets              = { 0 };
        textRes->indexBufferOffset    = 0;
        textRes->indexCount           = 32;
        textRes->descSet              = m_fonts.at(fontId)->descSet;
        textRes->updateStagingBuffer(text, *m_fonts.at(fontId)->font.get());

        m_textResources.emplace_back(std::move(textRes));

        return freeTextResourceId;
    }

    glm::vec2 RenderSystem::updateTextResource(unsigned int textResId, const std::string& text, unsigned int fontId)
    {
        m_textResources.at(textResId)->descSet = m_fonts.at(fontId)->descSet;
        m_textResources.at(textResId)->updateStagingBuffer(text, *m_fonts.at(fontId)->font);
        m_textResources.at(textResId)->isUpdatedOnDevice = false;
        return m_textResources.at(textResId)->extent;
    }

    void RenderSystem::unregisterTextResource(unsigned int textResId)
    {
        m_textResourceIdPool.insert(textResId);
    }

    glm::vec2 RenderSystem::queryTextExtent(std::string text, unsigned int fontId) const
    {
        glm::vec2 extent = glm::vec2(0.0f, 0.0f);
        auto font = m_fonts.at(fontId)->font.get();
        for (auto& character : text)
        {
            auto& gInfo = font->glyphs[character - FontLoader::CharBegin];
            extent.x += gInfo.advanceX;
            extent.y = std::max(extent.y, gInfo.bmpHeight);
        }
        return extent;
    }

    void RenderSystem::drawQuad(unsigned int transformResourceId, uint32_t colorId, float depth) const
    {
        m_drawCommands.emplace_back(&RenderSystem::renderQuad, transformResourceId, colorId, depth);
    }

    void RenderSystem::drawTexture(unsigned int transformId, unsigned int colorId, unsigned int texCoordId, float depth) const
    {
        m_drawCommands.emplace_back(&RenderSystem::renderTexture, transformId, colorId, texCoordId, depth);
    }

    void RenderSystem::drawText(unsigned int textResId, unsigned int transformId, uint32_t colorId, float depth) const
    {
        m_drawCommands.emplace_back(&RenderSystem::renderText, transformId, colorId, textResId, depth);
    }

    void RenderSystem::drawDebugRect(Rect<float> rect, glm::vec4 color) const
    {
        m_debugRects.emplace_back(rect);
        m_rectColors.emplace_back(color);
    }

    void RenderSystem::submitDrawCommands()
    {
        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer commandBuffer)
        {
            auto currentFrame = m_renderer->getCurrentVirtualFrameIndex();

            m_transforms->update(commandBuffer, currentFrame);
            m_colors->update(commandBuffer, currentFrame);
            m_tcTransforms->update(commandBuffer, currentFrame);

            // Text resources
            for (auto& textResource : m_textResources)
            {
                auto textRes = textResource.get();
                if (!textRes->isUpdatedOnDevice)
                {
                    textRes->updatedBufferIndex = (textRes->updatedBufferIndex + 1) % RendererConfig::VirtualFrameCount;
                    textRes->offsets[0] = textRes->updatedBufferIndex * textRes->allocatedVertexCount * sizeof(glm::vec4);
                    textRes->indexBufferOffset = textRes->updatedBufferIndex * textRes->allocatedFaceCount * sizeof(glm::u16vec3);
                    textRes->vertexBuffer->updateDeviceBuffer(commandBuffer, textRes->updatedBufferIndex);
                    textRes->indexBuffer->updateDeviceBuffer(commandBuffer, textRes->updatedBufferIndex);

                    textRes->isUpdatedOnDevice = true;
                }
            }
        });

        m_renderer->enqueueDrawCommand([this](VkCommandBuffer commandBuffer)
        {
            std::sort(m_drawCommands.begin(), m_drawCommands.end(), [](const GuiDrawCommand& a, const GuiDrawCommand& b)
            {
                return a.depth < b.depth;
            });

            auto currentFrame = m_renderer->getCurrentVirtualFrameIndex();

            m_guiPass->begin(commandBuffer);
            for (auto& cmd : m_drawCommands)
            {
                (this->*(cmd.drawFuncPtr))(commandBuffer, currentFrame, cmd);
            }

            for (int i = 0; i < m_debugRects.size(); i++) {
                renderDebugRect(commandBuffer, m_debugRects[i], m_rectColors[i]);
            }



            m_guiPass->end(commandBuffer, currentFrame);

            auto size = m_drawCommands.size();
            m_drawCommands.clear();
            m_drawCommands.reserve(size);

            m_debugRects.clear();
            m_rectColors.clear();

            m_guiPass->getRenderTarget(0)->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, currentFrame, 1,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });

        m_renderer->enqueueDefaultPassDrawCommand([this](VkCommandBuffer commandBuffer)
        {
            unsigned int frameIndex = m_renderer->getCurrentVirtualFrameIndex();
            m_fsQuadPipeline->bind(commandBuffer);
            m_fsMaterial->bind(frameIndex, commandBuffer);

            m_renderer->setDefaultViewport(commandBuffer);
            m_renderer->setDefaultScissor(commandBuffer);
            m_renderer->drawFullScreenQuad(commandBuffer);
        });
    }

    void RenderSystem::resize(int /*width*/, int /*height*/)
    {
        m_P = glm::ortho(0.0f, static_cast<float>(m_renderer->getSwapChainExtent().width), 0.0f, static_cast<float>(m_renderer->getSwapChainExtent().height), 0.5f, 0.5f + DepthLayers);

        m_guiPass->recreate();
        updateFullScreenMaterial();
    }

    const Renderer& RenderSystem::getRenderer() const
    {
        return *m_renderer;
    }

    glm::vec2 RenderSystem::getScreenSize() const
    {
        return { m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height };
    }

    uint32_t RenderSystem::getFont(std::string name, uint32_t pixelSize)
    {
        for (uint32_t i = 0; i < m_fonts.size(); ++i)
        {
            auto& font = m_fonts[i]->font;
            if (font->name == name && font->pixelSize == pixelSize)
                return i;
        }

        auto font = m_fontLoader.load(m_renderer->getResourcesPath() / "Fonts" / name, pixelSize);
        auto fontTexture = std::make_unique<FontTexture>();
        fontTexture->font = std::move(font);

        auto numChannels = 1;
        auto width    = fontTexture->font->width;
        auto height   = fontTexture->font->height;
        auto byteSize = width * height  * numChannels * sizeof(unsigned char);

        fontTexture->texture = std::make_unique<Texture>(m_renderer, VkExtent3D{ width, height, 1 }, 1, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        fontTexture->texture->fill(fontTexture->font->textureData.data(), byteSize);
        fontTexture->VulkanImageView = fontTexture->texture->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1);

        auto imageInfo = fontTexture->VulkanImageView->getDescriptorInfo(m_linearClampSampler.get());

        fontTexture->descSet = m_textPipeline->allocateDescriptorSet(1).getHandle();

        std::vector<VkWriteDescriptorSet> descWrites(2, VkWriteDescriptorSet{});
        descWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[0].dstSet          = fontTexture->descSet;
        descWrites[0].dstBinding      = 0;
        descWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        descWrites[0].dstArrayElement = 0;
        descWrites[0].descriptorCount = 1;
        descWrites[0].pImageInfo      = &imageInfo;

        descWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[1].dstSet          = fontTexture->descSet;
        descWrites[1].dstBinding      = 1;
        descWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descWrites[1].dstArrayElement = 0;
        descWrites[1].descriptorCount = 1;
        descWrites[1].pImageInfo      = &imageInfo;

        vkUpdateDescriptorSets(m_device->getHandle(), static_cast<uint32_t>(descWrites.size()), descWrites.data(), 0, nullptr);

        m_fonts.emplace_back(std::move(fontTexture));

        return static_cast<uint32_t>(m_fonts.size() - 1);
    }

    void RenderSystem::createPipelines()
    {
        m_colorQuadPipeline = m_renderer->createPipelineFromLua("GuiColor.lua", *m_guiPass.get(), 0);
        m_textPipeline      = m_renderer->createPipelineFromLua("GuiText.lua", *m_guiPass.get(), 0);
        m_texQuadPipeline   = m_renderer->createPipelineFromLua("GuiTexture.lua", *m_guiPass.get(), 0);
        m_debugRectPipeline = m_renderer->createPipelineFromLua("GuiDebug.lua", *m_guiPass.get(), 0);
        m_fsQuadPipeline    = m_renderer->createPipelineFromLua("Fullscreen.lua", *m_renderer->getDefaultRenderPass(), 0);
    }

    void RenderSystem::initGeometryBuffers()
    {
        std::vector<glm::vec2> quadVerts  = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
        std::vector<glm::uvec3> quadFaces = { { 0, 2, 1 }, { 0, 3, 2 } };
        m_quadGeometry = std::make_unique<Geometry>(m_renderer, quadVerts, quadFaces);

        std::vector<glm::uvec2> loopSegments = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 } };
        m_lineLoopGeometry = std::make_unique<Geometry>(m_renderer, quadVerts, loopSegments);
    }

    void RenderSystem::initGuiRenderTargetResources()
    {
        m_fsMaterial = std::make_unique<Material>(m_fsQuadPipeline.get());
        updateFullScreenMaterial();
    }

    void RenderSystem::updateFullScreenMaterial()
    {
        m_fsMaterial->writeDescriptor(0, 0, *m_guiPass, 0, m_linearClampSampler.get());
        m_device->flushDescriptorUpdates();
    }

    void RenderSystem::loadTextureAtlas()
    {
        const auto image = loadImage(m_renderer->getResourcesPath() / "Textures/Gui/Atlas.png").unwrap();
        m_guiAtlas     = std::make_unique<Texture>(m_renderer, VkExtent3D{ image.getWidth(), image.getHeight(), 1u }, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        m_guiAtlasView = m_guiAtlas->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1);
        m_guiAtlas->fill(image.getData(), image.getByteSize());
    }

    void RenderSystem::renderQuad(VkCommandBuffer cmdBuffer, uint32_t frameIdx, const GuiDrawCommand& cmd) const
    {
        m_colorQuadPipeline->bind(cmdBuffer);
        m_renderer->setDefaultViewport(cmdBuffer);
        m_renderer->setDefaultScissor(cmdBuffer);

        // Latest updated buffer
        VkDescriptorSet descSets[] =
        {
            m_transforms->getDescriptorSet(frameIdx)
        };
        uint32_t dynamicOffsets[] =
        {
            m_transforms->getDynamicOffset(cmd.transformId),
            m_colors->getDynamicOffset(cmd.colorId)
        };
        uint32_t pushConstants[2] =
        {
            m_transforms->getPushConstantValue(cmd.transformId),
            m_colors->getPushConstantValue(cmd.colorId)
        };

        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_colorQuadPipeline->getPipelineLayout()->getHandle(),
            0, 1, descSets, 2, dynamicOffsets);
        m_colorQuadPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_VERTEX_BIT, 0, pushConstants[0]);
        m_colorQuadPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(int), pushConstants[1]);

        m_quadGeometry->bindAndDraw(cmdBuffer);
    }

    void RenderSystem::renderText(VkCommandBuffer cmdBuffer, uint32_t frameIdx, const GuiDrawCommand& cmd) const
    {
        const auto textRes = m_textResources.at(cmd.textId).get();
        m_textPipeline->bind(cmdBuffer);
        m_renderer->setDefaultViewport(cmdBuffer);
        m_renderer->setDefaultScissor(cmdBuffer);

        VkDescriptorSet descSets[] =
        {
            m_transforms->getDescriptorSet(frameIdx),
            textRes->descSet
        };
        uint32_t dynamicOffsets[] =
        {
            m_transforms->getDynamicOffset(cmd.transformId),
            m_colors->getDynamicOffset(cmd.colorId)
        };
        int pushConstants[2] =
        {
            static_cast<int>(m_transforms->getPushConstantValue(cmd.transformId)),
            static_cast<int>(m_colors->getPushConstantValue(cmd.colorId))
        };

        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_textPipeline->getPipelineLayout()->getHandle(),
            0, 2, descSets, 2, dynamicOffsets);

        vkCmdPushConstants(cmdBuffer, m_textPipeline->getPipelineLayout()->getHandle(), VK_SHADER_STAGE_VERTEX_BIT,             0, sizeof(int), &pushConstants[0]);
        vkCmdPushConstants(cmdBuffer, m_textPipeline->getPipelineLayout()->getHandle(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(int), sizeof(int), &pushConstants[1]);

        textRes->drawIndexed(cmdBuffer);
    }

    void RenderSystem::renderTexture(VkCommandBuffer cmdBuffer, uint32_t frameIdx, const GuiDrawCommand& cmd) const
    {
        m_texQuadPipeline->bind(cmdBuffer);
        m_renderer->setDefaultViewport(cmdBuffer);
        m_renderer->setDefaultScissor(cmdBuffer);

        VkDescriptorSet descSets[] =
        {
            m_transforms->getDescriptorSet(frameIdx),
            m_tcTransforms->getDescriptorSet(frameIdx)
        };
        uint32_t dynamicOffsets[] =
        {
            m_transforms->getDynamicOffset(cmd.transformId),
            m_colors->getDynamicOffset(cmd.colorId),
            m_tcTransforms->getDynamicOffset(cmd.textId)
        };
        int pushConstants[3] =
        {
            static_cast<int>(m_transforms->getPushConstantValue(cmd.transformId)),
            static_cast<int>(m_colors->getPushConstantValue(cmd.colorId)),
            static_cast<int>(m_tcTransforms->getPushConstantValue(cmd.textId))
        };

        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_texQuadPipeline->getPipelineLayout()->getHandle(),
            0, 2, descSets, 3, dynamicOffsets);
        vkCmdPushConstants(cmdBuffer, m_texQuadPipeline->getPipelineLayout()->getHandle(), VK_SHADER_STAGE_VERTEX_BIT,             0,     sizeof(int), &pushConstants[0]);
        vkCmdPushConstants(cmdBuffer, m_texQuadPipeline->getPipelineLayout()->getHandle(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(int), 2 * sizeof(int), &pushConstants[1]);

        m_quadGeometry->bindAndDraw(cmdBuffer);
    }

    void RenderSystem::renderDebugRect(VkCommandBuffer cmdBuffer, const Rect<float>& rect, const glm::vec4& color) const
    {
        glm::mat4 transform = glm::translate(glm::vec3{ rect.x, rect.y, -1.0f }) * glm::scale(glm::vec3{ rect.width, rect.height, 1.0f });
        m_debugRectPipeline->bind(cmdBuffer);
        m_debugRectPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_VERTEX_BIT, 0, m_P * transform);
        m_debugRectPipeline->setPushConstant(cmdBuffer, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), color);
        m_lineLoopGeometry->bindAndDraw(cmdBuffer);
    }

    void RenderSystem::TextGeometryResource::updateStagingBuffer(std::string text, const Font& font)
    {
        std::vector<glm::vec4> textVertices;
        std::vector<glm::u16vec3> textFaces;

        unsigned short ind = 0;
        float currentX = 0.f;
        float currentY = 0.f;
        float atlasWidth  = static_cast<float>(font.width);
        float atlasHeight = static_cast<float>(font.height);

        extent = glm::vec2(0.0f, 0.0f);
        for (auto& character : text)
        {
            auto& gInfo = font.glyphs[character - FontLoader::CharBegin];
            float x1 = currentX + gInfo.bmpLeft;
            float y1 = currentY - gInfo.bmpTop;
            float x2 = x1 + gInfo.bmpWidth;
            float y2 = y1 + gInfo.bmpHeight;

            // Advance the cursor to the start of the next character
            currentX += gInfo.advanceX;
            currentY += gInfo.advanceY;

            textVertices.emplace_back(x1, y1, gInfo.atlasOffsetX, 0.0f);
            textVertices.emplace_back(x2, y1, gInfo.atlasOffsetX + gInfo.bmpWidth / atlasWidth, 0.0f);
            textVertices.emplace_back(x2, y2, gInfo.atlasOffsetX + gInfo.bmpWidth / atlasWidth, gInfo.bmpHeight / atlasHeight);
            textVertices.emplace_back(x1, y2, gInfo.atlasOffsetX, gInfo.bmpHeight / atlasHeight);

            textFaces.emplace_back(ind + 0, ind + 2, ind + 1);
            textFaces.emplace_back(ind + 0, ind + 3, ind + 2);

            ind += 4;

            extent.y = std::max(extent.y, gInfo.bmpHeight);
        }
        extent.x += currentX;

        vertexCount = static_cast<uint32_t>(textVertices.size());
        indexCount = static_cast<uint32_t>(textFaces.size()) * 3;

        vertexBuffer->updateStagingBuffer(textVertices);
        indexBuffer->updateStagingBuffer(textFaces);
    }

    void RenderSystem::TextGeometryResource::drawIndexed(VkCommandBuffer cmdBuffer) const
    {
        vkCmdBindVertexBuffers(cmdBuffer, firstBinding, bindingCount, buffers.data(), offsets.data());
        indexBuffer->bind(cmdBuffer, indexBufferOffset);
        vkCmdDrawIndexed(cmdBuffer, indexCount, 1, 0, 0, 0);
    }
}