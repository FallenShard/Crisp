#include "RenderSystem.hpp"
#define NOMINMAX

#include <algorithm>
#include <iostream>

#include "Vulkan/VulkanRenderer.hpp"
#include "vulkan/Pipelines/VulkanPipeline.hpp"
#include "IO/FontLoader.hpp"

#include "vulkan/Pipelines/FullScreenQuadPipeline.hpp"
#include "vulkan/Pipelines/GuiColorQuadPipeline.hpp"
#include "vulkan/Pipelines/GuiTextPipeline.hpp"
#include "vulkan/Pipelines/GuiTexQuadPipeline.hpp"
#include "vulkan/RenderPasses/GuiRenderPass.hpp"

#include "GUI/Control.hpp"
#include "GUI/Panel.hpp"
#include "GUI/Label.hpp"
#include "GUI/Button.hpp"
#include "GUI/CheckBox.hpp"

#include "IO/Image.hpp"

namespace crisp
{
    namespace gui
    {
        RenderSystem::RenderSystem(VulkanRenderer* renderer)
            : m_renderer(renderer)
            , m_device(renderer->getDevice())
        {
            auto width  = static_cast<float>(m_renderer->getSwapChainExtent().width);
            auto height = static_cast<float>(m_renderer->getSwapChainExtent().height);
            m_P = glm::ortho(0.0f, width, 0.0f, height, 0.5f, 0.5f + DepthLayers);

            // Create the render pass where all GUI controls will be drawn
            m_guiPass = std::make_unique<GuiRenderPass>(m_renderer);

            // Create pipelines for different types of drawable objects
            createPipelines();
            
            // [0..1] Quad geometries for GUI elements
            initGeometryBuffers();

            // Initialize resources to support dynamic addition of MVP transform resources
            initTransformsResources();

            // Initialize resources to support dynamic addition of color resources
            initColorResources();

            // Initialize gui render target rendering resources
            m_linearClampSampler = m_device->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
            initGuiRenderTargetResources();

            m_nnClampSampler = m_device->createSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

            //loadTextures();
            
            // Register the render pass into the renderer's draw loop
            m_renderer->registerRenderPass(GuiRenderPassId, m_guiPass.get());
        }

        RenderSystem::~RenderSystem()
        {
            //vkDestroyImageView(m_renderer->getDevice().getHandle(), m_checkBoxImageView, nullptr);

            for (auto& font : m_fonts)
                vkDestroyImageView(m_device->getHandle(), font->imageView, nullptr);

            vkDestroySampler(m_device->getHandle(), m_nnClampSampler, nullptr);
            vkDestroySampler(m_device->getHandle(), m_linearClampSampler, nullptr);
            vkDestroyImageView(m_device->getHandle(), m_guiImageView, nullptr);
        }

        const glm::mat4& RenderSystem::getProjectionMatrix() const
        {
            return m_P;
        }

        unsigned int RenderSystem::registerTransformResource()
        {
            auto freeTransformId = *m_transformsResourceIdPool.begin(); // Smallest element in a set is at .begin()
            m_transformsResourceIdPool.erase(freeTransformId);
            m_numRegisteredTransformResources++;

            // If we use up all free resource ids, resize the buffers
            if (m_transformsResourceIdPool.empty())
            {
                for (unsigned int i = 0; i < MatricesPerGranularity; i++)
                {
                    m_transformsResourceIdPool.insert(m_numRegisteredTransformResources + i);
                }

                auto numTransforms = m_numRegisteredTransformResources + m_transformsResourceIdPool.size();
                auto multiplier = (numTransforms - 1) / MatricesPerGranularity + 1;

                // Create the new staging buffer, prepare previous one to be destroyed and set the new one as main
                auto prevSize = m_transformsStagingBufferSize;
                m_transformsStagingBufferSize = UniformBufferGranularity * multiplier;

                auto newStagingBuffer = m_device->createStagingBuffer(m_transformsStagingBuffer, prevSize, m_transformsStagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                m_renderer->scheduleStagingBufferForRemoval(m_transformsStagingBuffer);
                m_transformsStagingBuffer = newStagingBuffer;

                // Invalidate current device-backed uniform buffers
                for (auto& res : m_transformsResources)
                {
                    res.isUpdated = false;
                }
            }

            return freeTransformId;
        }

        void RenderSystem::updateTransformResource(unsigned int transformId, const glm::mat4& M)
        {
            m_device->updateStagingBuffer(m_transformsStagingBuffer, glm::value_ptr(m_P * M), transformId * MatrixSize, MatrixSize);
        }

        void RenderSystem::unregisterTransformResource(unsigned int transformId)
        {
            m_transformsResourceIdPool.insert(transformId);
            m_numRegisteredTransformResources--;
        }

        unsigned int RenderSystem::registerColorResource()
        {
            auto freeColorId = *m_colorsResourceIdPool.begin(); // Smallest element in a set is at .begin()
            m_colorsResourceIdPool.erase(freeColorId);
            m_numRegisteredColorResources++;

            // If we use up all free resource ids, resize the buffers
            if (m_colorsResourceIdPool.empty())
            {
                for (unsigned int i = 0; i < MatricesPerGranularity; i++)
                {
                    m_colorsResourceIdPool.insert(m_numRegisteredColorResources + i);
                }

                auto numTransforms = m_numRegisteredColorResources + m_colorsResourceIdPool.size();
                auto multiplier = (numTransforms - 1) / ColorsPerGranularity + 1;

                // Create the new staging buffer, prepare previous one to be destroyed and set the new one as main
                auto prevSize = m_colorsStagingBufferSize;
                m_colorsStagingBufferSize = UniformBufferGranularity * multiplier;

                auto newStagingBuffer = m_device->createStagingBuffer(m_colorsStagingBuffer, prevSize, m_colorsStagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                m_renderer->scheduleStagingBufferForRemoval(m_colorsStagingBuffer);
                m_colorsStagingBuffer = newStagingBuffer;

                // Invalidate current device-backed uniform buffers
                for (auto& res : m_colorsResources)
                {
                    res.isUpdated = false;
                }
            }

            return freeColorId;
        }
        
        void RenderSystem::updateColorResource(unsigned int colorId, const glm::vec4& color)
        {
            m_device->updateStagingBuffer(m_colorsStagingBuffer, glm::value_ptr(color), colorId * ColorSize, ColorSize);
        }

        void RenderSystem::unregisterColorResource(unsigned int colorId)
        {
            m_colorsResourceIdPool.insert(colorId);
            m_numRegisteredColorResources--;
        }

        unsigned int RenderSystem::registerTexCoordResource()
        {
            return 0;
        }

        void RenderSystem::updateTexCoordResource(unsigned int, const glm::vec2&, const glm::vec2&)
        {
            //m_texCoordResources.at(resourceId)->generate(min, max, m_renderer);
            //m_texCoordResources.at(resourceId)->needsUpdateToDevice = true;
        }

        void RenderSystem::unregisterTexCoordResource(unsigned int)
        {
            //m_freeTexCoordResourceIds.insert(resourceId);
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
            textRes->allocatedVertexCount        = TextGeometryResource::NumInitialAllocatedCharacters * 4; // 4 Vertices per letter
            textRes->allocatedFaceCount          = TextGeometryResource::NumInitialAllocatedCharacters * 2; // 2 Triangles per letter
            textRes->updatedBufferIndex          = 0;
            textRes->isUpdatedOnDevice           = false;
            textRes->geomData.vertexBuffer       = std::make_unique<VertexBuffer>(m_device, textRes->allocatedVertexCount * sizeof(glm::vec4), BufferUpdatePolicy::PerFrame);
            textRes->geomData.indexBuffer        = std::make_unique<IndexBuffer>(m_device, VK_INDEX_TYPE_UINT16, BufferUpdatePolicy::PerFrame, textRes->allocatedFaceCount * sizeof(glm::u16vec3));
            textRes->geomData.vertexBufferGroup  = { { textRes->geomData.vertexBuffer->get(), 0 } };
            textRes->geomData.indexBufferOffset  = 0;
            textRes->geomData.indexCount         = 32;
            textRes->descSets.emplace_back(m_fonts.at(fontId)->descSet);
            textRes->updateStagingBuffer(text, m_fonts.at(fontId)->font.get(), m_renderer);

            m_textResources.emplace_back(std::move(textRes));

            return freeTextResourceId;
        }

        glm::vec2 RenderSystem::updateTextResource(unsigned int textResId, const std::string& text, unsigned int fontId)
        {
            m_textResources.at(textResId)->descSets[0] = m_fonts.at(fontId)->descSet;
            m_textResources.at(textResId)->updateStagingBuffer(text, m_fonts.at(fontId)->font.get(), m_renderer);
            m_textResources.at(textResId)->isUpdatedOnDevice = false;
            return m_textResources.at(textResId)->extent;
        }

        void RenderSystem::unregisterTextResource(unsigned int textResId)
        {
            m_textResourceIdPool.insert(textResId);
        }

        glm::vec2 RenderSystem::queryTextExtent(std::string text, unsigned int fontId)
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

        void RenderSystem::draw(const CheckBox&)
        {
            //auto texRes = m_texCoordResources.at(checkBox.getTexCoordResourceId()).get();
            //m_drawCommands.emplace_back(
            //    m_texQuadPipeline.get(),
            //    m_texQuadDescSets.data(),
            //    1, 1,
            //    &texRes->geom,
            //    checkBox.getTransformId(),
            //    checkBox.getColor(),
            //    checkBox.getDepth()
            //);
        }

        void RenderSystem::drawQuad(unsigned int transformResourceId, uint32_t colorId, float depth) const
        {
            m_drawCommands.emplace_back(m_colorQuadPipeline.get(), nullptr, 0, 0, &m_quadGeometry,
                static_cast<uint16_t>(transformResourceId), static_cast<uint16_t>(colorId), depth);
        }

        void RenderSystem::drawText(unsigned int textRenderResourceId, unsigned int transformResourceId, uint32_t colorId, float depth)
        {
            auto textRes = m_textResources.at(textRenderResourceId).get();
            m_drawCommands.emplace_back(m_textPipeline.get(), textRes->descSets.data(), static_cast<uint8_t>(1), static_cast<uint8_t>(textRes->descSets.size()), &textRes->geomData,
                static_cast<uint16_t>(transformResourceId), static_cast<uint16_t>(colorId), depth);
        }

        void RenderSystem::submitDrawRequests()
        {
            m_renderer->addCopyAction([this](VkCommandBuffer& commandBuffer)
            {
                auto currentFrame = m_renderer->getCurrentVirtualFrameIndex();

                // Update device uniform buffer for the current command buffer
                auto& currTransRes = m_transformsResources[currentFrame];

                if (!currTransRes.isUpdated)
                {
                    updateTransformUniformBuffer(currentFrame);
                }

                VkBufferCopy copyRegion = {};
                copyRegion.srcOffset = 0;
                copyRegion.dstOffset = 0;
                copyRegion.size = m_transformsStagingBufferSize;
                vkCmdCopyBuffer(commandBuffer, m_transformsStagingBuffer, currTransRes.uniformBuffer, 1, &copyRegion);

                // Color
                auto& currColorRes = m_colorsResources[currentFrame];

                if (!currColorRes.isUpdated)
                {
                    updateColorBuffer(currentFrame);
                }

                copyRegion = {};
                copyRegion.srcOffset = 0;
                copyRegion.dstOffset = 0;
                copyRegion.size = m_colorsStagingBufferSize;
                vkCmdCopyBuffer(commandBuffer, m_colorsStagingBuffer, currColorRes.uniformBuffer, 1, &copyRegion);

                // Text resources
                for (auto& textResource : m_textResources)
                {
                    auto textRes = textResource.get();
                    if (!textRes->isUpdatedOnDevice)
                    {
                        textRes->updatedBufferIndex = (textRes->updatedBufferIndex + 1) % VulkanRenderer::NumVirtualFrames;
                        textRes->geomData.vertexBufferGroup.offsets[0] = textRes->updatedBufferIndex * textRes->allocatedVertexCount * sizeof(glm::vec4);
                        textRes->geomData.indexBufferOffset = textRes->updatedBufferIndex * textRes->allocatedFaceCount * sizeof(glm::u16vec3);
                        textRes->geomData.vertexBuffer->updateDeviceBuffer(commandBuffer, textRes->updatedBufferIndex);
                        textRes->geomData.indexBuffer->updateDeviceBuffer(commandBuffer, textRes->updatedBufferIndex);

                        textRes->isUpdatedOnDevice = true;
                    }
                }

                //for (auto& texCoordRes : m_texCoordResources)
                //{
                //    auto texRes = texCoordRes.get();
                //    if (texRes->needsUpdateToDevice)
                //    {
                //        texRes->updatedBufferIndex = (texRes->updatedBufferIndex + 1) % VulkanRenderer::NumVirtualFrames;
                //        texRes->geom.vertexBufferOffset = texRes->updatedBufferIndex * 4 * sizeof(glm::vec4);
                //        VkBufferCopy copyVertexRegion = {};
                //        copyVertexRegion.srcOffset = 0;
                //        copyVertexRegion.dstOffset = texRes->geom.vertexBufferOffset;
                //        copyVertexRegion.size = 4 * sizeof(glm::vec4);
                //        vkCmdCopyBuffer(commandBuffer, texRes->stagingVertexBuffer, texRes->geom.vertexBuffer, 1, &copyVertexRegion);
                //
                //        texRes->needsUpdateToDevice = false;
                //    }
                //}
            });

            m_renderer->addDrawAction([this](VkCommandBuffer& commandBuffer)
            {
                std::sort(m_drawCommands.begin(), m_drawCommands.end(), [](const GuiDrawCommand& a, const GuiDrawCommand& b)
                {
                    return a.depth < b.depth;
                });

                auto currentFrame = m_renderer->getCurrentVirtualFrameIndex();

                for (auto& cmd : m_drawCommands)
                {
                    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.pipeline->getHandle());
                    if (cmd.descriptorSets)
                        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.pipeline->getPipelineLayout(),
                            cmd.firstSet, cmd.descSetCount, cmd.descriptorSets, 0, nullptr);
                    
                    // Latest updated buffer
                    uint32_t dynamicOffsets[2];
                    dynamicOffsets[0] = static_cast<uint32_t>(UniformBufferGranularity * (cmd.transformId / MatricesPerGranularity)); // order is important!
                    dynamicOffsets[1] = static_cast<uint32_t>(UniformBufferGranularity * (cmd.colorId / ColorsPerGranularity)); // order is important!

                    int pushConstants[2];
                    pushConstants[0] = cmd.transformId % MatricesPerGranularity;
                    pushConstants[1] = cmd.colorId % ColorsPerGranularity;

                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.pipeline->getPipelineLayout(),
                        0, 1, &m_transformsResources[currentFrame].descSet, 2, dynamicOffsets);
                
                    vkCmdPushConstants(commandBuffer, cmd.pipeline->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(int), &pushConstants[0]);
                    vkCmdPushConstants(commandBuffer, cmd.pipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(int), sizeof(int), &pushConstants[1]);
                
                    cmd.geom->vertexBufferGroup.bind(commandBuffer);
                    cmd.geom->indexBuffer->bind(commandBuffer, cmd.geom->indexBufferOffset);
                    vkCmdDrawIndexed(commandBuffer, cmd.geom->indexCount, 1, 0, 0, 0);
                }

                auto size = m_drawCommands.size();
                m_drawCommands.clear();
                m_drawCommands.reserve(size);

            }, GuiRenderPassId);

            m_renderer->addDrawAction([this](VkCommandBuffer& commandBuffer)
            {
                VkViewport viewport;
                viewport.x = viewport.y = 0;
                viewport.width  = static_cast<float>(m_renderer->getSwapChainExtent().width);
                viewport.height = static_cast<float>(m_renderer->getSwapChainExtent().height);
                viewport.minDepth = 0;
                viewport.maxDepth = 1;
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_fsQuadPipeline->getHandle());
                vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_fsQuadPipeline->getPipelineLayout(),
                    0, 1, &m_fsQuadDescSet, 0, nullptr);

                unsigned int pushConst = m_renderer->getCurrentVirtualFrameIndex();
                vkCmdPushConstants(commandBuffer, m_fsQuadPipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(unsigned int), &pushConst);
                
                m_renderer->drawFullScreenQuad(commandBuffer);
            }, VulkanRenderer::DefaultRenderPassId);
        }

        void RenderSystem::resize(int width, int height)
        {
            m_P = glm::ortho(0.0f, static_cast<float>(m_renderer->getSwapChainExtent().width), 0.0f, static_cast<float>(m_renderer->getSwapChainExtent().height), 0.5f, 0.5f + DepthLayers);

            m_guiPass->recreate();
            m_colorQuadPipeline->resize(width, height);
            m_textPipeline->resize(width, height);
            m_texQuadPipeline->resize(width, height);
            m_fsQuadPipeline->resize(width, height);

            vkDestroyImageView(m_device->getHandle(), m_guiImageView, nullptr);
            m_guiImageView = m_device->createImageView(m_guiPass->getColorAttachment(0), VK_IMAGE_VIEW_TYPE_2D_ARRAY, m_guiPass->getColorFormat(), VK_IMAGE_ASPECT_COLOR_BIT, 0, VulkanRenderer::NumVirtualFrames);

            VkDescriptorImageInfo imageInfo = {};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView   = m_guiImageView;

            std::array<VkWriteDescriptorSet, 1> descriptorWrites = {};

            descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet          = m_fsQuadDescSet;
            descriptorWrites[0].dstBinding      = 1;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo      = &imageInfo;

            vkUpdateDescriptorSets(m_device->getHandle(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }

        //    //
        //    //// Dynamic tex coords
        //    //auto numTexCoords = *m_freeTexCoordResourceIds.begin();
        //    //m_texCoordResources.reserve(numTexCoords);
        //    //for (unsigned int i = 0; i < numTexCoords; i++)
        //    //{
        //    //    m_texCoordResources.emplace_back(std::make_unique<TexCoordResource>());
        //    //    m_texCoordResources[i]->geom.vertexBuffer   = m_renderer->getDevice().createDeviceBuffer(VulkanRenderer::NumVirtualFrames * 4 * sizeof(glm::vec4), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        //    //    m_texCoordResources[i]->stagingVertexBuffer = m_renderer->getDevice().createStagingBuffer(4 * sizeof(glm::vec4), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        //    //    m_texCoordResources[i]->geom.indexBuffer    = m_quadGeometry.indexBuffer;
        //    //    m_texCoordResources[i]->geom.indexBufferOffset = 0;
        //    //    m_texCoordResources[i]->geom.vertexBufferOffset = 0;
        //    //    m_texCoordResources[i]->geom.indexCount = 6;
        //    //    m_texCoordResources[i]->updatedBufferIndex = 0;
        //    //    m_texCoordResources[i]->needsUpdateToDevice = true;
        //    //}
        //}

        DeviceMemoryMetrics RenderSystem::getDeviceMemoryUsage()
        {
            return m_device->getDeviceMemoryUsage();
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

            auto font = m_fontLoader.load(name, pixelSize);
            auto fontTexture = std::make_unique<FontTexture>();
            fontTexture->font = std::move(font);

            auto numChannels = 1;
            auto width    = fontTexture->font->width;
            auto height   = fontTexture->font->height;
            auto byteSize = width * height  * numChannels * sizeof(unsigned char);

            fontTexture->image = m_device->createDeviceImage(width, height, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
            m_device->fillDeviceImage(fontTexture->image, fontTexture->font->textureData.data(), byteSize, { width, height, 1u }, 1);
            m_device->transitionImageLayout(fontTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
            fontTexture->imageView = m_device->createImageView(fontTexture->image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);

            VkDescriptorImageInfo imageInfo = {};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView   = fontTexture->imageView;
            imageInfo.sampler     = m_linearClampSampler;

            fontTexture->descSet = m_textPipeline->allocateDescriptorSet(GuiTextPipeline::FontAtlas);

            std::vector<VkWriteDescriptorSet> descWrites(2, VkWriteDescriptorSet{});
            descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descWrites[0].dstSet = fontTexture->descSet;
            descWrites[0].dstBinding = 0;
            descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
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
            m_colorQuadPipeline = std::make_unique<GuiColorQuadPipeline>(m_renderer, m_guiPass.get());
            m_textPipeline      = std::make_unique<GuiTextPipeline>(m_renderer, m_guiPass.get());
            m_texQuadPipeline   = std::make_unique<GuiTexQuadPipeline>(m_renderer, m_guiPass.get());

            m_fsQuadPipeline    = std::make_unique<FullScreenQuadPipeline>(m_renderer, m_renderer->getDefaultRenderPass());
        }

        void RenderSystem::initGeometryBuffers()
        {
            // Vertex buffer
            std::vector<glm::vec2> quadVerts = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
            m_quadGeometry.vertexBuffer = std::make_unique<VertexBuffer>(m_device, quadVerts);

            // Index buffer
            std::vector<glm::u16vec3> quadFaces = { { 0, 1, 2 }, { 0, 2, 3 } };
            m_quadGeometry.indexBuffer = std::make_unique<IndexBuffer>(m_device, quadFaces);

            m_quadGeometry.vertexBufferGroup = {
                { m_quadGeometry.vertexBuffer->get(), 0 }
            };

            m_quadGeometry.indexBufferOffset  = 0;
            m_quadGeometry.indexCount         = 6;
        }

        void RenderSystem::initTransformsResources()
        {
            for (unsigned int i = 0; i < MatricesPerGranularity; i++)
            {
                m_transformsResourceIdPool.insert(i); // unoccupied mvp transform ids
            }

            // Minimum multiples of granularity to store TransformsResourceIncrement matrices
            auto multiplier = (m_transformsResourceIdPool.size() - 1) / MatricesPerGranularity + 1;

            // Single staging buffer, will always contain latest data
            m_transformsStagingBufferSize = UniformBufferGranularity * multiplier;
            m_transformsStagingBuffer     = m_device->createStagingBuffer(m_transformsStagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

            // Per-frame buffers and descriptor sets, because each unprocessed command buffer may hold onto its own unique version when the current one is modified
            for (unsigned int i = 0; i < VulkanRenderer::NumVirtualFrames; i++)
            {
                auto& res = m_transformsResources[i];

                // Create the descriptor set
                res.descSet = m_colorQuadPipeline->allocateDescriptorSet(GuiColorQuadPipeline::TransformAndColor);

                // Create the buffer and link it with descriptor set
                res.createBufferAndUpdateSet(m_device, m_transformsStagingBufferSize, 0);
            }
        }

        void RenderSystem::initColorResources()
        {
            for (unsigned int i = 0; i < ColorsPerGranularity; i++)
            {
                m_colorsResourceIdPool.insert(i); // unoccupiedcolor ids
            }

            // Minimum multiples of granularity to store colors
            auto multiplier = (m_colorsResourceIdPool.size() - 1) / ColorsPerGranularity + 1;

            m_colorsStagingBufferSize = UniformBufferGranularity * multiplier;
            m_colorsStagingBuffer     = m_device->createStagingBuffer(m_colorsStagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

            // Per-frame buffers and descriptor sets, because each unprocessed command buffer may hold onto its own unique version when the current one is modified
            for (unsigned int i = 0; i < VulkanRenderer::NumVirtualFrames; i++)
            {
                auto& res = m_colorsResources[i];

                res.descSet = m_transformsResources[i].descSet;

                // Create the buffer and link it with descriptor set
                res.createBufferAndUpdateSet(m_device, m_colorsStagingBufferSize, 1);
            }
        }

        void RenderSystem::initGuiRenderTargetResources()
        {
            // Create descriptor set for the image
            m_fsQuadDescSet = m_fsQuadPipeline->allocateDescriptorSet(FullScreenQuadPipeline::DisplayedImage);

            // Create a view to the render target
            m_guiImageView = m_device->createImageView(m_guiPass->getColorAttachment(), VK_IMAGE_VIEW_TYPE_2D_ARRAY, m_guiPass->getColorFormat(), VK_IMAGE_ASPECT_COLOR_BIT, 0, VulkanRenderer::NumVirtualFrames);
            
            VkDescriptorImageInfo imageInfo = {};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView   = m_guiImageView;
            imageInfo.sampler     = m_linearClampSampler;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};
            descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet          = m_fsQuadDescSet;
            descriptorWrites[0].dstBinding      = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo      = &imageInfo;

            descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet          = m_fsQuadDescSet;
            descriptorWrites[1].dstBinding      = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo      = &imageInfo;

            vkUpdateDescriptorSets(m_device->getHandle(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }

        void RenderSystem::loadTextures()
        {
            //auto cbImage = std::make_unique<Image>("Resources/Textures/Gui/check-box.png");
            //auto numChannels = cbImage->getNumComponents();
            //auto width = cbImage->getWidth();
            //auto height = cbImage->getHeight();
            //auto byteSize = width * height  * numChannels * sizeof(unsigned char);
            //
            //m_checkBoxImage = m_renderer->getDevice().createDeviceImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
            //m_renderer->getDevice().fillDeviceImage(m_checkBoxImage, cbImage->getData(), byteSize, { width, height, 1u }, 1);
            //m_renderer->getDevice().transitionImageLayout(m_checkBoxImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
            //m_checkBoxImageView = m_renderer->getDevice().createImageView(m_checkBoxImage, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
            //
            //// CheckBoxSampler
            //VkSamplerCreateInfo samplerInfo = {};
            //samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            //samplerInfo.magFilter               = VK_FILTER_LINEAR;
            //samplerInfo.minFilter               = VK_FILTER_LINEAR;
            //samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            //samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            //samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            //samplerInfo.anisotropyEnable        = VK_FALSE;
            //samplerInfo.maxAnisotropy           = 1.0f;
            //samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            //samplerInfo.unnormalizedCoordinates = VK_FALSE;
            //samplerInfo.compareEnable           = VK_FALSE;
            //samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
            //samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            //samplerInfo.mipLodBias              = 0.0f;
            //samplerInfo.minLod                  = 0.0f;
            //samplerInfo.maxLod                  = 0.0f;
            //vkCreateSampler(m_renderer->getDevice().getHandle(), &samplerInfo, nullptr, &m_checkBoxSampler);
        }

        void RenderSystem::updateTransformUniformBuffer(uint32_t frameId)
        {
            auto& res = m_transformsResources[frameId];
            
            // Destroy the old buffer to free memory 
            m_device->destroyDeviceBuffer(res.uniformBuffer);

            // Create a new buffer and link it to descriptor set
            res.createBufferAndUpdateSet(m_device, m_transformsStagingBufferSize, 0);
        }

        void RenderSystem::updateColorBuffer(uint32_t frameId)
        {
            auto& res = m_colorsResources[frameId];

            // Destroy the old buffer to free memory 
            m_device->destroyDeviceBuffer(res.uniformBuffer);

            // Create a new buffer and link it to descriptor set
            res.createBufferAndUpdateSet(m_device, m_transformsStagingBufferSize, 1);
        }

        void RenderSystem::UniformBufferFrameResource::createBufferAndUpdateSet(VulkanDevice* device, VkDeviceSize size, uint32_t binding)
        {
            bufferSize = size;
            uniformBuffer = device->createDeviceBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

            VkDescriptorBufferInfo transBufferInfo = {};
            transBufferInfo.buffer = uniformBuffer;
            transBufferInfo.offset = 0;
            transBufferInfo.range  = UniformBufferGranularity; // Because we use UNIFORM_BUFFER_DYNAMIC, it stays the same size

            VkWriteDescriptorSet descWrite = {};
            descWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descWrite.dstSet          = descSet;
            descWrite.dstBinding      = binding;
            descWrite.dstArrayElement = 0;
            descWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            descWrite.descriptorCount = 1;
            descWrite.pBufferInfo     = &transBufferInfo;

            vkUpdateDescriptorSets(device->getHandle(), 1, &descWrite, 0, nullptr);
            isUpdated = true;
        }

        void RenderSystem::TextGeometryResource::updateStagingBuffer(std::string text, Font* font, VulkanRenderer* renderer)
        {
            std::vector<glm::vec4> textVertices;
            std::vector<glm::u16vec3> textFaces;

            unsigned short ind = 0;
            float currentX = 0.f;
            float currentY = 0.f;
            float atlasWidth  = static_cast<float>(font->width);
            float atlasHeight = static_cast<float>(font->height);

            extent = glm::vec2(0.0f, 0.0f);
            for (auto& character : text)
            {
                auto& gInfo = font->glyphs[character - FontLoader::CharBegin];
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
            faceCount   = static_cast<uint32_t>(textFaces.size());
            geomData.indexCount = faceCount * 3;

            geomData.vertexBuffer->updateStagingBuffer(textVertices);
            geomData.indexBuffer->updateStagingBuffer(textFaces);
        }

        //void RenderSystem::TexCoordResource::generate(const glm::vec2& min, const glm::vec2& max, VulkanRenderer* renderer)
        //{
        //    std::vector<glm::vec4> vertices;
        //    vertices.emplace_back(0.0f, 0.0f, min.x, min.y);
        //    vertices.emplace_back(1.0f, 0.0f, max.x, min.y);
        //    vertices.emplace_back(1.0f, 1.0f, max.x, max.y);
        //    vertices.emplace_back(0.0f, 1.0f, min.x, max.y);

        //    renderer->getDevice().fillStagingBuffer(stagingVertexBuffer, vertices.data(), vertices.size() * sizeof(glm::vec4));
        //}

        
    }
}