#include "RenderSystem.hpp"
#define NOMINMAX

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include "Vulkan/VulkanRenderer.hpp"
#include "vulkan/Pipelines/VulkanPipeline.hpp"
#include "ShaderLoader.hpp"
#include "FontLoader.hpp"

#include "vulkan/Pipelines/GuiColorQuadPipeline.hpp"
#include "vulkan/Pipelines/GuiTextPipeline.hpp"
#include "vulkan/Pipelines/GuiTexQuadPipeline.hpp"

#include "GUI/Control.hpp"
#include "GUI/Panel.hpp"
#include "GUI/Label.hpp"
#include "GUI/Button.hpp"
#include "GUI/CheckBox.hpp"

#include "Core/Image.hpp"

namespace crisp
{
    namespace gui
    {
        RenderSystem::RenderSystem(VulkanRenderer* renderer)
            : m_renderer(renderer)
        {
            m_P = glm::ortho(0.0f, static_cast<float>(m_renderer->getSwapChainExtent().width),
                0.0f, static_cast<float>(m_renderer->getSwapChainExtent().height),
                0.5f, 32.5f);

            for (int i = 0; i < 100; i++)
            {
                m_freeTransformIds.insert(i);
                m_freeTextResourceIds.insert(i);
                m_freeTexCoordResourceIds.insert(i);
            }

            loadFonts();
            loadTextures();
            createPipelines();
            initColorPaletteBuffer();
            initGeometryBuffers();

            updateDescriptorSets();

            m_drawCommands.reserve(100);
        }

        RenderSystem::~RenderSystem()
        {
            vkDestroySampler(m_renderer->getDevice().getHandle(), m_fontSampler, nullptr);
            vkDestroySampler(m_renderer->getDevice().getHandle(), m_checkBoxSampler, nullptr);

            vkDestroyImageView(m_renderer->getDevice().getHandle(), m_checkBoxImageView, nullptr);

            for (auto& font : m_fonts)
            {
                vkDestroyImageView(m_renderer->getDevice().getHandle(), font.second->imageView, nullptr);
            }
        }

        const glm::mat4& RenderSystem::getProjectionMatrix() const
        {
            return m_P;
        }

        unsigned int RenderSystem::registerTransformResource()
        {
            auto freeId = *m_freeTransformIds.begin();
            m_freeTransformIds.erase(freeId);

            return freeId;
        }

        void RenderSystem::updateTransformResource(unsigned int transformId, const glm::mat4& M)
        {
            m_mvpTransforms[transformId / 4].MVP[transformId % 4] = m_P * M;
        }

        void RenderSystem::unregisterTransformResource(unsigned int transformId)
        {
            m_freeTransformIds.insert(transformId);
        }

        unsigned int RenderSystem::registerTexCoordResource()
        {
            auto freeId = *m_freeTexCoordResourceIds.begin();
            m_freeTexCoordResourceIds.erase(freeId);

            return freeId;
        }

        void RenderSystem::updateTexCoordResource(unsigned int resourceId, const glm::vec2& min, const glm::vec2& max)
        {
            m_texCoordResources.at(resourceId)->generate(min, max, m_renderer);
            m_texCoordResources.at(resourceId)->needsUpdateToDevice = true;
        }

        void RenderSystem::unregisterTexCoordResource(unsigned int resourceId)
        {
            m_freeTexCoordResourceIds.insert(resourceId);
        }

        unsigned int RenderSystem::registerTextResource()
        {
            auto freeId = *m_freeTextResourceIds.begin();
            m_freeTextResourceIds.erase(freeId);
            return freeId;
        }

        glm::vec2 RenderSystem::updateTextResource(unsigned int textResId, const std::string& text)
        {
            std::string clippedString = text.size() > 32 ? text.substr(0, 32) : text;
            m_textResources.at(textResId)->text = clippedString;
            m_textResources.at(textResId)->updateStagingBuffer(m_labelFont->font.get(), m_renderer);
            m_textResources.at(textResId)->needsUpdateToDevice = true;

            return m_textResources.at(textResId)->extent;
        }

        void RenderSystem::unregisterTextResource(unsigned int textResId)
        {
            m_freeTextResourceIds.insert(textResId);
        }

        glm::vec2 RenderSystem::queryTextExtent(std::string text)
        {
            glm::vec2 extent = glm::vec2(0.0f, 0.0f);
            float currentX = 0.0f;
            for (auto& character : text)
            {
                auto& gInfo = m_labelFont->font->glyphs[character - FontLoader::CharBegin];
                currentX += gInfo.advanceX;
                extent.y = std::max(extent.y, gInfo.bmpHeight);
            }
            extent.x += currentX;
            return extent;
        }

        void RenderSystem::draw(const Panel& panel)
        {
            m_drawCommands.emplace_back(
                m_colorQuadPipeline.get(),
                m_colorQuadDescSets.data(),
                1, 1,
                &m_quadGeometry,
                panel.getTransformId(),
                panel.getColor(),
                panel.getDepth()
            );
        }

        void RenderSystem::draw(const Button& button)
        {
            m_drawCommands.emplace_back(
                m_colorQuadPipeline.get(),
                m_colorQuadDescSets.data(),
                1, 1,
                &m_quadGeometry,
                button.getTransformId(),
                button.getColor(),
                button.getDepth()
            );
        }

        void RenderSystem::draw(const CheckBox& checkBox)
        {
            auto texRes = m_texCoordResources.at(checkBox.getTexCoordResourceId()).get();
            m_drawCommands.emplace_back(
                m_texQuadPipeline.get(),
                m_texQuadDescSets.data(),
                1, 1,
                &texRes->geom,
                checkBox.getTransformId(),
                checkBox.getColor(),
                checkBox.getDepth()
            );
        }

        void RenderSystem::draw(const Label& label)
        {
            auto textRes = m_textResources.at(label.getTextResourceId()).get();
            m_drawCommands.emplace_back(
                m_textPipeline.get(),
                m_textDescSets.data(),
                1, 1,
                &textRes->geomData,
                label.getTransformId(),
                label.getColor(),
                label.getDepth()
            );
        }

        void RenderSystem::submitDrawRequests()
        {
            m_renderer->getDevice().fillStagingBuffer(m_transStagingBuffer, m_mvpTransforms.data(), m_transBufferSize);

            m_renderer->addCopyAction([this](VkCommandBuffer& commandBuffer)
            {
                m_updatedTransBufferIndex = (m_updatedTransBufferIndex + 1) % VulkanRenderer::NumVirtualFrames;
                VkBufferCopy copyRegion = {};
                copyRegion.srcOffset = 0;
                copyRegion.dstOffset = m_updatedTransBufferIndex * m_transBufferSize;
                copyRegion.size = m_transBufferSize;
                vkCmdCopyBuffer(commandBuffer, m_transStagingBuffer, m_transBuffer, 1, &copyRegion);

                for (auto& textResource : m_textResources)
                {
                    auto textRes = textResource.get();
                    if (textRes->needsUpdateToDevice)
                    {
                        textRes->updatedBufferIndex = (textRes->updatedBufferIndex + 1) % VulkanRenderer::NumVirtualFrames;
                        textRes->geomData.vertexBufferOffset = textRes->updatedBufferIndex * sizeof(glm::vec4) * textRes->allocatedVertexCount;
                        textRes->geomData.indexBufferOffset = textRes->updatedBufferIndex * sizeof(glm::u16vec3) * textRes->allocatedFaceCount;
                        VkBufferCopy copyVertexRegion = {};
                        copyVertexRegion.srcOffset = 0;
                        copyVertexRegion.dstOffset = textRes->geomData.vertexBufferOffset;
                        copyVertexRegion.size = sizeof(glm::vec4) * textRes->vertexCount;
                        vkCmdCopyBuffer(commandBuffer, textRes->stagingVertexBuffer, textRes->geomData.vertexBuffer, 1, &copyVertexRegion);

                        VkBufferCopy copyIndexRegion = {};
                        copyIndexRegion.srcOffset = 0;
                        copyIndexRegion.dstOffset = textRes->geomData.indexBufferOffset;
                        copyIndexRegion.size = sizeof(glm::u16vec3) * textRes->faceCount;
                        vkCmdCopyBuffer(commandBuffer, textRes->stagingIndexBuffer, textRes->geomData.indexBuffer, 1, &copyIndexRegion);

                        textRes->needsUpdateToDevice = false;
                    }
                }

                for (auto& texCoordRes : m_texCoordResources)
                {
                    auto texRes = texCoordRes.get();
                    if (texRes->needsUpdateToDevice)
                    {
                        texRes->updatedBufferIndex = (texRes->updatedBufferIndex + 1) % VulkanRenderer::NumVirtualFrames;
                        texRes->geom.vertexBufferOffset = texRes->updatedBufferIndex * 4 * sizeof(glm::vec4);
                        VkBufferCopy copyVertexRegion = {};
                        copyVertexRegion.srcOffset = 0;
                        copyVertexRegion.dstOffset = texRes->geom.vertexBufferOffset;
                        copyVertexRegion.size = 4 * sizeof(glm::vec4);
                        vkCmdCopyBuffer(commandBuffer, texRes->stagingVertexBuffer, texRes->geom.vertexBuffer, 1, &copyVertexRegion);

                        texRes->needsUpdateToDevice = false;
                    }
                }
            });

            m_renderer->addDrawAction([this](VkCommandBuffer& commandBuffer)
            {
                std::sort(m_drawCommands.begin(), m_drawCommands.end(), [](const GuiDrawCommand& a, const GuiDrawCommand& b)
                {
                    return a.depth < b.depth;
                });

                for (auto& cmd : m_drawCommands)
                {
                    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.pipeline->getPipeline());
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.pipeline->getPipelineLayout(),
                        cmd.firstSet, cmd.descSetCount, &cmd.descriptorSets[1], 0, nullptr);
                    
                    uint32_t dynamicOffset = m_updatedTransBufferIndex * m_transBufferSize  + sizeof(GuiTransform) * (cmd.transformId / MatricesPerUniformBuffer);
                    int mvpIndex = cmd.transformId % MatricesPerUniformBuffer;

                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cmd.pipeline->getPipelineLayout(),
                        0, 1, &m_transformSet, 1, &dynamicOffset);

                    vkCmdPushConstants(commandBuffer, cmd.pipeline->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(int), &mvpIndex);
                    vkCmdPushConstants(commandBuffer, cmd.pipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(int), sizeof(int), &cmd.colorIndex);

                    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &cmd.geom->vertexBuffer, &cmd.geom->vertexBufferOffset);
                    vkCmdBindIndexBuffer(commandBuffer, cmd.geom->indexBuffer, cmd.geom->indexBufferOffset, VK_INDEX_TYPE_UINT16);
                    vkCmdDrawIndexed(commandBuffer, cmd.geom->indexCount, 1, 0, 0, 0);
                }

                m_drawCommands.clear();
            });
        }

        void RenderSystem::resize(int width, int height)
        {
            m_P = glm::ortho(0.0f, static_cast<float>(m_renderer->getSwapChainExtent().width),
                0.0f, static_cast<float>(m_renderer->getSwapChainExtent().height),
                0.5f, 32.5f);

            m_colorQuadPipeline->resize(width, height);
            m_textPipeline->resize(width, height);
        }

        void RenderSystem::buildResourceBuffers()
        {
            // Transforms
            auto numObjects = *m_freeTransformIds.begin();
            auto numStructs = (numObjects - 1) / MatricesPerUniformBuffer + 1;
            m_transBufferSize = UniformBufferGranularity * numStructs;
            m_mvpTransforms.resize(numStructs);

            m_transBuffer = m_renderer->getDevice().createDeviceBuffer(VulkanRenderer::NumVirtualFrames * m_transBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            m_transStagingBuffer = m_renderer->getDevice().createStagingBuffer(m_transBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

            for (int i = 0; i < VulkanRenderer::NumVirtualFrames; i++)
                m_renderer->getDevice().fillDeviceBuffer(m_transBuffer, m_mvpTransforms.data(), i * m_transBufferSize, m_transBufferSize);
            m_renderer->getDevice().fillStagingBuffer(m_transStagingBuffer, m_mvpTransforms.data(), m_transBufferSize);

            m_updatedTransBufferIndex = 0;

            VkDescriptorBufferInfo transBufferInfo = {};
            transBufferInfo.buffer = m_transBuffer;
            transBufferInfo.offset = 0;
            transBufferInfo.range = sizeof(GuiTransform);

            std::vector<VkWriteDescriptorSet> descWrites(1, {});
            descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descWrites[0].dstSet = m_colorQuadDescSets[0];
            descWrites[0].dstBinding = 0;
            descWrites[0].dstArrayElement = 0;
            descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            descWrites[0].descriptorCount = 1;
            descWrites[0].pBufferInfo = &transBufferInfo;

            vkUpdateDescriptorSets(m_renderer->getDevice().getHandle(), static_cast<uint32_t>(descWrites.size()), descWrites.data(), 0, nullptr);

            // Texts
            auto numLabels = *m_freeTextResourceIds.begin();
            m_textResources.reserve(numLabels);
            for (unsigned int i = 0; i < numLabels; i++)
            {
                m_textResources.emplace_back(std::make_unique<TextVertexResource>());
                m_textResources[i]->allocatedVertexCount        = 4 * 32; // 32 characters max per geometry, 4 vertices per character
                m_textResources[i]->allocatedFaceCount          = 2 * 32; // 2 faces per character
                m_textResources[i]->updatedBufferIndex          = 0;
                m_textResources[i]->needsUpdateToDevice         = true;
                m_textResources[i]->text                        = "Vilasini";
                m_textResources[i]->geomData.vertexBuffer       = m_renderer->getDevice().createDeviceBuffer(VulkanRenderer::NumVirtualFrames * m_textResources[i]->allocatedVertexCount * sizeof(glm::vec4), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
                m_textResources[i]->stagingVertexBuffer         = m_renderer->getDevice().createStagingBuffer(m_textResources[i]->allocatedVertexCount * sizeof(glm::vec4), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                m_textResources[i]->geomData.indexBuffer        = m_renderer->getDevice().createDeviceBuffer(VulkanRenderer::NumVirtualFrames * sizeof(glm::u16vec3) * m_textResources[i]->allocatedFaceCount, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
                m_textResources[i]->stagingIndexBuffer          = m_renderer->getDevice().createStagingBuffer(m_textResources[i]->allocatedFaceCount * sizeof(glm::u16vec3), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                m_textResources[i]->geomData.vertexBufferOffset = 0;
                m_textResources[i]->geomData.indexBufferOffset  = 0;
                m_textResources[i]->geomData.indexCount         = 48;
            }

            // Dynamic tex coords
            auto numTexCoords = *m_freeTexCoordResourceIds.begin();
            m_texCoordResources.reserve(numTexCoords);
            for (unsigned int i = 0; i < numTexCoords; i++)
            {
                m_texCoordResources.emplace_back(std::make_unique<TexCoordResource>());
                m_texCoordResources[i]->geom.vertexBuffer   = m_renderer->getDevice().createDeviceBuffer(VulkanRenderer::NumVirtualFrames * 4 * sizeof(glm::vec4), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
                m_texCoordResources[i]->stagingVertexBuffer = m_renderer->getDevice().createStagingBuffer(4 * sizeof(glm::vec4), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                m_texCoordResources[i]->geom.indexBuffer    = m_quadGeometry.indexBuffer;
                m_texCoordResources[i]->geom.indexBufferOffset = 0;
                m_texCoordResources[i]->geom.vertexBufferOffset = 0;
                m_texCoordResources[i]->geom.indexCount = 6;
                m_texCoordResources[i]->updatedBufferIndex = 0;
                m_texCoordResources[i]->needsUpdateToDevice = true;
            }
        }

        std::pair<uint64_t, uint64_t> RenderSystem::getDeviceMemoryUsage()
        {
            return m_renderer->getDevice().getDeviceMemoryUsage();
        }

        void RenderSystem::createPipelines()
        {
            m_colorQuadPipeline = std::make_unique<GuiColorQuadPipeline>(m_renderer);
            m_textPipeline = std::make_unique<GuiTextPipeline>(m_renderer);
            m_texQuadPipeline = std::make_unique<GuiTexQuadPipeline>(m_renderer);

            m_transformSet = m_colorQuadPipeline->allocateDescriptorSet(0);
            m_colorQuadDescSets.emplace_back(m_transformSet);
            m_colorQuadDescSets.emplace_back(m_colorQuadPipeline->allocateDescriptorSet(1));

            m_textDescSets.emplace_back(m_transformSet);
            m_textDescSets.emplace_back(m_textPipeline->allocateDescriptorSet(1));

            m_texQuadDescSets.emplace_back(m_transformSet);
            m_texQuadDescSets.emplace_back(m_texQuadPipeline->allocateDescriptorSet(1));
        }

        void RenderSystem::initGeometryBuffers()
        {
            // Vertex buffer
            std::vector<glm::vec2> quadVerts = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
            m_quadGeometry.vertexBuffer = m_renderer->getDevice().createDeviceBuffer(quadVerts.size() * sizeof(glm::vec2), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            m_renderer->getDevice().fillDeviceBuffer(m_quadGeometry.vertexBuffer, quadVerts.data(), quadVerts.size() * sizeof(glm::vec2));

            // Index buffer
            std::vector<glm::u16vec3> quadFaces = { { 0, 1, 2 }, { 0, 2, 3 } };
            m_quadGeometry.indexBuffer = m_renderer->getDevice().createDeviceBuffer(sizeof(glm::u16vec3) * quadFaces.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
            m_renderer->getDevice().fillDeviceBuffer(m_quadGeometry.indexBuffer, quadFaces.data(), sizeof(glm::u16vec3) * quadFaces.size());

            m_quadGeometry.vertexBufferOffset = 0;
            m_quadGeometry.indexBufferOffset = 0;
            m_quadGeometry.indexCount = 6;
        }

        void RenderSystem::updateDescriptorSets()
        {
            VkDescriptorBufferInfo colorBufferInfo = {};
            colorBufferInfo.buffer = m_colorPaletteBuffer;
            colorBufferInfo.offset = 0;
            colorBufferInfo.range  = UniformBufferGranularity;

            std::vector<VkWriteDescriptorSet> descWrites(1, {});
            descWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descWrites[0].dstSet          = m_colorQuadDescSets.at(1);
            descWrites[0].dstBinding      = 0;
            descWrites[0].dstArrayElement = 0;
            descWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descWrites[0].descriptorCount = 1;
            descWrites[0].pBufferInfo     = &colorBufferInfo;
            vkUpdateDescriptorSets(m_renderer->getDevice().getHandle(), static_cast<uint32_t>(descWrites.size()), descWrites.data(), 0, nullptr);

            VkDescriptorImageInfo imageInfo = {};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = m_labelFont->imageView;
            imageInfo.sampler = m_fontSampler;

            VkCopyDescriptorSet descSetCopy = {};
            descSetCopy.sType           = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
            descSetCopy.srcSet          = m_colorQuadDescSets.at(1);
            descSetCopy.srcBinding      = 0;
            descSetCopy.srcArrayElement = 0;
            descSetCopy.dstSet          = m_textDescSets.at(1);
            descSetCopy.dstBinding      = 2;
            descSetCopy.dstArrayElement = 0;
            descSetCopy.descriptorCount = 1;

            std::vector<VkWriteDescriptorSet> textDescWrites(2, {});
            for (auto& descWrite : textDescWrites)
            {
                descWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descWrite.dstArrayElement = 0;
                descWrite.dstSet          = m_textDescSets[1];
            }

            textDescWrites[0].dstBinding      = 0;
            textDescWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
            textDescWrites[0].descriptorCount = 1;
            textDescWrites[0].pImageInfo      = &imageInfo;

            textDescWrites[1].dstBinding      = 1;
            textDescWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            textDescWrites[1].descriptorCount = 1;
            textDescWrites[1].pImageInfo      = &imageInfo;
            vkUpdateDescriptorSets(m_renderer->getDevice().getHandle(), static_cast<uint32_t>(textDescWrites.size()), textDescWrites.data(), 1, &descSetCopy);

            // TexQuad desc set
            VkCopyDescriptorSet texCoordDescSetCopy = {};
            texCoordDescSetCopy.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
            texCoordDescSetCopy.srcSet = m_colorQuadDescSets.at(1);
            texCoordDescSetCopy.srcBinding = 0;
            texCoordDescSetCopy.srcArrayElement = 0;
            texCoordDescSetCopy.dstSet = m_texQuadDescSets.at(1);
            texCoordDescSetCopy.dstBinding = 2;
            texCoordDescSetCopy.dstArrayElement = 0;
            texCoordDescSetCopy.descriptorCount = 1;

            std::vector<VkWriteDescriptorSet> texQuadWrites(2, {});
            for (auto& descWrite : texQuadWrites)
            {
                descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descWrite.dstArrayElement = 0;
                descWrite.dstSet = m_texQuadDescSets[1];
            }

            VkDescriptorImageInfo cbImageInfo = {};
            cbImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            cbImageInfo.imageView = m_checkBoxImageView;
            cbImageInfo.sampler = m_checkBoxSampler;

            texQuadWrites[0].dstBinding = 0;
            texQuadWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            texQuadWrites[0].descriptorCount = 1;
            texQuadWrites[0].pImageInfo = &cbImageInfo;
            texQuadWrites[1].dstBinding = 1;
            texQuadWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            texQuadWrites[1].descriptorCount = 1;
            texQuadWrites[1].pImageInfo = &cbImageInfo;
            vkUpdateDescriptorSets(m_renderer->getDevice().getHandle(), static_cast<uint32_t>(texQuadWrites.size()), texQuadWrites.data(), 1, &texCoordDescSetCopy);
        }

        void RenderSystem::initColorPaletteBuffer()
        {
            std::vector<glm::vec4> colors(UniformBufferGranularity / sizeof(glm::vec4));
            colors.at(DarkGray)   = glm::vec4(0.15f, 0.15f, 0.15f, 0.8f);
            colors.at(Green)      = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
            colors.at(LightGreen) = glm::vec4(0.5f, 1.0f, 0.5f, 1.0f);
            colors.at(DarkGreen)  = glm::vec4(0.0f, 0.5f, 0.0f, 1.0f);
            colors.at(LightGray)  = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
            colors.at(White)      = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            colors.at(MediumGray) = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
            colors.at(Gray20)     = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
            colors.at(Gray30)     = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);
            colors.at(Gray40)     = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);

            m_colorPaletteBuffer = m_renderer->getDevice().createDeviceBuffer(colors.size() * sizeof(glm::vec4), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            m_renderer->getDevice().fillDeviceBuffer(m_colorPaletteBuffer, colors.data(), colors.size() * sizeof(glm::vec4));
        }

        void RenderSystem::loadFonts()
        {
            FontLoader fontLoader;
            auto font = fontLoader.load("SegoeUI.ttf", 14);

            auto fontTexture = std::make_unique<FontTexture>();
            fontTexture->font = std::move(font.second);

            auto numChannels = 1;
            auto width = fontTexture->font->width;
            auto height = fontTexture->font->height;
            auto byteSize = width * height  * numChannels * sizeof(unsigned char);

            fontTexture->image = m_renderer->getDevice().createDeviceImage(width, height, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
            m_renderer->getDevice().fillDeviceImage(fontTexture->image, fontTexture->font->textureData.data(), byteSize, { width, height, 1u }, 1);
            m_renderer->getDevice().transitionImageLayout(fontTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
            fontTexture->imageView = m_renderer->getDevice().createImageView(fontTexture->image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);

            m_labelFont = fontTexture.get();
            m_fonts.insert_or_assign(font.first, std::move(fontTexture));

            // Font sampler
            VkSamplerCreateInfo samplerInfo = {};
            samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter               = VK_FILTER_LINEAR;
            samplerInfo.minFilter               = VK_FILTER_LINEAR;
            samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.anisotropyEnable        = VK_FALSE;
            samplerInfo.maxAnisotropy           = 1.0f;
            samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;
            samplerInfo.compareEnable           = VK_FALSE;
            samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
            samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.mipLodBias              = 0.0f;
            samplerInfo.minLod                  = 0.0f;
            samplerInfo.maxLod                  = 0.0f;
            vkCreateSampler(m_renderer->getDevice().getHandle(), &samplerInfo, nullptr, &m_fontSampler);
        }

        void RenderSystem::loadTextures()
        {
            auto cbImage = std::make_unique<Image>("Resources/Textures/Gui/check-box.png");
            auto numChannels = cbImage->getNumComponents();
            auto width = cbImage->getWidth();
            auto height = cbImage->getHeight();
            auto byteSize = width * height  * numChannels * sizeof(unsigned char);

            m_checkBoxImage = m_renderer->getDevice().createDeviceImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
            m_renderer->getDevice().fillDeviceImage(m_checkBoxImage, cbImage->getData(), byteSize, { width, height, 1u }, 1);
            m_renderer->getDevice().transitionImageLayout(m_checkBoxImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
            m_checkBoxImageView = m_renderer->getDevice().createImageView(m_checkBoxImage, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);

            // Font sampler
            VkSamplerCreateInfo samplerInfo = {};
            samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter               = VK_FILTER_LINEAR;
            samplerInfo.minFilter               = VK_FILTER_LINEAR;
            samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.anisotropyEnable        = VK_FALSE;
            samplerInfo.maxAnisotropy           = 1.0f;
            samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;
            samplerInfo.compareEnable           = VK_FALSE;
            samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
            samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.mipLodBias              = 0.0f;
            samplerInfo.minLod                  = 0.0f;
            samplerInfo.maxLod                  = 0.0f;
            vkCreateSampler(m_renderer->getDevice().getHandle(), &samplerInfo, nullptr, &m_checkBoxSampler);
        }

        void RenderSystem::TextVertexResource::updateStagingBuffer(Font* font, VulkanRenderer* renderer)
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

                textVertices.emplace_back(glm::vec4 { x1, y1, gInfo.atlasOffsetX, 0.0f });
                textVertices.emplace_back(glm::vec4 { x2, y1, gInfo.atlasOffsetX + gInfo.bmpWidth / atlasWidth, 0.0f });
                textVertices.emplace_back(glm::vec4 { x2, y2, gInfo.atlasOffsetX + gInfo.bmpWidth / atlasWidth, gInfo.bmpHeight / atlasHeight });
                textVertices.emplace_back(glm::vec4 { x1, y2, gInfo.atlasOffsetX, gInfo.bmpHeight / atlasHeight });

                textFaces.emplace_back(glm::u16vec3 { ind + 0, ind + 2, ind + 1 });
                textFaces.emplace_back(glm::u16vec3 { ind + 0, ind + 3, ind + 2 });

                ind += 4;

                extent.y = std::max(extent.y, gInfo.bmpHeight);
            }
            extent.x += currentX;

            vertexCount = static_cast<uint32_t>(textVertices.size());
            faceCount   = static_cast<uint32_t>(textFaces.size());
            geomData.indexCount = faceCount * 3;

            renderer->getDevice().fillStagingBuffer(stagingVertexBuffer, textVertices.data(), vertexCount * sizeof(glm::vec4));
            renderer->getDevice().fillStagingBuffer(stagingIndexBuffer, textFaces.data(), faceCount * sizeof(glm::u16vec3));
        }

        void RenderSystem::TexCoordResource::generate(const glm::vec2& min, const glm::vec2& max, VulkanRenderer* renderer)
        {
            std::vector<glm::vec4> vertices;
            vertices.emplace_back(0.0f, 0.0f, min.x, min.y);
            vertices.emplace_back(1.0f, 0.0f, max.x, min.y);
            vertices.emplace_back(1.0f, 1.0f, max.x, max.y);
            vertices.emplace_back(0.0f, 1.0f, min.x, max.y);

            renderer->getDevice().fillStagingBuffer(stagingVertexBuffer, vertices.data(), vertices.size() * sizeof(glm::vec4));
        }
    }
}