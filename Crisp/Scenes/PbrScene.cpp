#include "PbrScene.hpp"

#include "Core/LuaConfig.hpp"
#include "Core/Application.hpp"
#include "Core/Window.hpp"

#include "Camera/CameraController.hpp"

#include "Vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "Vulkan/VulkanSampler.hpp"
#include "Vulkan/VulkanPipeline.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/UniformBuffer.hpp"
#include "Renderer/Material.hpp"
#include "Renderer/RenderGraph.hpp"
#include "Renderer/VulkanBufferUtils.hpp"
#include "Renderer/VulkanImageUtils.hpp"
#include "Renderer/ResourceContext.hpp"
#include "Renderer/RenderPasses/ShadowPass.hpp"
#include "Renderer/RenderPasses/ForwardLightingPass.hpp"
#include "Renderer/RenderPasses/AtmosphericLutPass.hpp"

#include "Models/Skybox.hpp"
#include "Geometry/TriangleMesh.hpp"
#include "Geometry/Geometry.hpp"
#include "Geometry/MeshGenerators.hpp"
#include "Geometry/TransformBuffer.hpp"
#include "Geometry/VertexLayout.hpp"

#include "Lights/LightSystem.hpp"
#include "Lights/DirectionalLight.hpp"
#include "Lights/EnvironmentLighting.hpp"

#include "GUI/Form.hpp"
#include "GUI/Label.hpp"
#include "GUI/CheckBox.hpp"
#include "GUI/Button.hpp"
#include "GUI/ComboBox.hpp"
#include "GUI/Slider.hpp"

#include <CrispCore/Math/Constants.hpp>
#include <CrispCore/Profiler.hpp>

namespace crisp
{
    namespace
    {
        static constexpr uint32_t ShadowMapSize = 2048;
        static constexpr uint32_t CascadeCount  = 4;

        static constexpr const char* MainPass = "mainPass";
        static constexpr const char* CsmPass  = "csmPass";
    }

    PbrScene::PbrScene(Renderer* renderer, Application* app)
        : m_renderer(renderer)
        , m_app(app)
        , m_resourceContext(std::make_unique<ResourceContext>(renderer))
        , m_shaderBallUVScale(1.0f)
    {
        setupInput();

        // Camera
        m_cameraController = std::make_unique<CameraController>(app->getWindow());
        m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);
        //m_cameraController->getCamera().setPosition(glm::vec3(5.0f, 5.0f, 5.0f));

        m_renderGraph = std::make_unique<RenderGraph>(m_renderer);

        // Main render pass
        m_renderGraph->addRenderPass(MainPass, std::make_unique<ForwardLightingPass>(m_renderer, VK_SAMPLE_COUNT_1_BIT));

        // Shadow map pass
        m_renderGraph->addRenderPass(CsmPass, std::make_unique<ShadowPass>(m_renderer, ShadowMapSize, CascadeCount));
        m_renderGraph->addRenderTargetLayoutTransition(CsmPass, MainPass, 0, CascadeCount);

        m_renderGraph->addRenderPass("TransLUTPass", std::make_unique<TransmittanceLutPass>(m_renderer));
        m_renderGraph->addRenderTargetLayoutTransition("TransLUTPass", MainPass, 0);

        // Wrap-up render graph definition
        m_renderGraph->addRenderTargetLayoutTransition(MainPass, "SCREEN", 0);
        m_renderGraph->sortRenderPasses();
        m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);

        m_lightSystem = std::make_unique<LightSystem>(m_renderer, ShadowMapSize);
        m_lightSystem->setDirectionalLight(DirectionalLight(-glm::vec3(1, 1, 0), glm::vec3(3.0f), glm::vec3(-5), glm::vec3(5)));
        m_lightSystem->enableCascadedShadowMapping(CascadeCount, ShadowMapSize);

        // Object transforms
        m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 100);

        createCommonTextures();

        for (uint32_t i = 0; i < CascadeCount; ++i)
        {
            std::string key = "cascadedShadowMap" + std::to_string(i);
            auto csmPipeline = m_resourceContext->createPipeline(key, "ShadowMap.lua", m_renderGraph->getRenderPass(CsmPass), i);
            auto csmMaterial = m_resourceContext->createMaterial(key, csmPipeline);
            csmMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
            csmMaterial->writeDescriptor(0, 1, *m_lightSystem->getCascadedDirectionalLightBuffer(i));
        }

        createPlane();
        createShaderballs();

        //auto transLutPipeline = m_resourceContext->createPipeline("transLut", "TransLut.lua", m_renderGraph->getRenderPass("TransLUTPass"), 0);
        /*auto transLutMaterial = m_resourceContext->createMaterial("transLut", transLutPipeline);

        auto transLutNode = std::make_unique<RenderNode>();
        transLutNode->geometry = m_renderer->getFullScreenGeometry();
        transLutNode->pass("TransLUTPass").material = transLutMaterial;*/
        //transLutNode->pass("TransLUTPass").setPushConstantView(m_ssaoParams);
        //m_renderNodes.emplace("transLutNode", std::move(transLutNode));


        m_skybox = std::make_unique<Skybox>(m_renderer, m_renderGraph->getRenderPass(MainPass), *m_resourceContext->getImageView("cubeMap"), *m_resourceContext->getSampler("linearClamp"));

        m_renderer->getDevice()->flushDescriptorUpdates();

        createGui(m_app->getForm());
    }

    PbrScene::~PbrScene()
    {
        m_renderer->setSceneImageView(nullptr, 0);
        m_app->getForm()->remove("shadowMappingPanel");
    }

    void PbrScene::resize(int width, int height)
    {
        m_cameraController->resize(width, height);

        m_renderGraph->resize(width, height);
        m_renderer->setSceneImageView(m_renderGraph->getNode(MainPass).renderPass.get(), 0);
    }

    void PbrScene::update(float dt)
    {
        m_cameraController->update(dt);
        const auto& V = m_cameraController->getViewMatrix();
        const auto& P = m_cameraController->getProjectionMatrix();

        m_transformBuffer->update(V, P);
        m_skybox->updateTransforms(V, P);

        m_lightSystem->update(m_cameraController->getCamera(), dt);

        //m_resourceContext->getUniformBuffer("pbrUnifParams")->updateStagingBuffer(m_uniformMaterialParams);

        m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(m_cameraController->getCameraParameters(), sizeof(CameraParameters));
    }

    void PbrScene::render()
    {
        m_renderGraph->clearCommandLists();
        m_renderGraph->buildCommandLists(m_renderNodes);
        m_renderGraph->addToCommandLists(m_skybox->getRenderNode());
        m_renderGraph->executeCommandLists();
    }

    void PbrScene::setRedAlbedo(double red)
    {
        m_uniformMaterialParams.albedo.r = static_cast<float>(red);
    }

    void PbrScene::setGreenAlbedo(double green)
    {
        m_uniformMaterialParams.albedo.g = static_cast<float>(green);
    }

    void PbrScene::setBlueAlbedo(double blue)
    {
        m_uniformMaterialParams.albedo.b = static_cast<float>(blue);
    }

    void PbrScene::setMetallic(double metallic)
    {
        m_uniformMaterialParams.metallic = static_cast<float>(metallic);
    }

    void PbrScene::setRoughness(double roughness)
    {
        m_uniformMaterialParams.roughness = static_cast<float>(roughness);
    }

    void PbrScene::setUScale(double uScale)
    {
        m_shaderBallUVScale.s = static_cast<float>(uScale);
    }

    void PbrScene::setVScale(double vScale)
    {
        m_shaderBallUVScale.t = static_cast<float>(vScale);
    }

    void PbrScene::onMaterialSelected(const std::string& materialName)
    {
        if (materialName == "Uniform")
        {
            m_renderNodes["pbrShaderBall"]->pass(MainPass).material = m_resourceContext->getMaterial("pbrUnif");
            return;
        }
        else
        {
            m_renderNodes["pbrShaderBall"]->pass(MainPass).material = m_resourceContext->getMaterial("pbrTexShaderBall");
        }

        m_renderer->finish();

        auto material = m_resourceContext->getMaterial("pbrTexShaderBall");

        VulkanSampler* linearRepeatSampler = m_resourceContext->getSampler("linearRepeat");
        const std::vector<std::string> texNames = { "diffuse", "metallic", "roughness", "normal", "ao" };
        const std::vector<VkFormat> formats = { VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };
        for (uint32_t i = 0; i < texNames.size(); ++i)
        {
            const std::string& filename = "PbrMaterials/" + materialName + "/" + texNames[i] + ".png";
            const auto& path = m_renderer->getResourcesPath() / "Textures" / filename;
            if (std::filesystem::exists(path))
            {
                std::string key = "shaderBall-" + texNames[i];
                m_resourceContext->addImageWithView(key, createTexture(m_renderer, filename, formats[i], true));
                material->writeDescriptor(1, 2 + i, *m_resourceContext->getImageView(key), linearRepeatSampler);
            }
            else
            {
                spdlog::warn("Texture type {} is using default values for '{}'", materialName, texNames[i]);
                std::string key = "default-" + texNames[i];
                material->writeDescriptor(1, 2 + i, *m_resourceContext->getImageView(key), linearRepeatSampler);
            }
        }
        m_renderer->getDevice()->flushDescriptorUpdates();
    }

    RenderNode* PbrScene::createRenderNode(std::string id, int transformIndex)
    {
        auto node = std::make_unique<RenderNode>(*m_transformBuffer, transformIndex);
        auto iterSuccessPair = m_renderNodes.emplace(id, std::move(node));
        return iterSuccessPair.first->second.get();
    }

    void PbrScene::createCommonTextures()
    {
        m_resourceContext->addSampler("nearestNeighbor", std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
        m_resourceContext->addSampler("linearRepeat", std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f, 12.0f));
        m_resourceContext->addSampler("linearMipmap", std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f, 5.0f));
        m_resourceContext->addSampler("linearClamp", std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f, 11.0f));
        m_resourceContext->addSampler("linearMirrorRepeat", std::make_unique<VulkanSampler>(m_renderer->getDevice(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, 16.0f, 11.0f));

        // For textured pbr
        //m_resourceContext->addImageWithView("default-diffuse", createTexture(m_renderer, 1, 1, { 255, 0, 255, 255 }, VK_FORMAT_R8G8B8A8_SRGB));
        m_resourceContext->addImageWithView("default-diffuse", createTexture(m_renderer, "uv_pattern.jpg", VK_FORMAT_R8G8B8A8_SRGB));
        m_resourceContext->addImageWithView("default-metallic", createTexture(m_renderer, 1, 1, { 0, 0, 0, 255 }, VK_FORMAT_R8G8B8A8_UNORM));
        m_resourceContext->addImageWithView("default-roughness", createTexture(m_renderer, 1, 1, { 32, 32, 32, 255 }, VK_FORMAT_R8G8B8A8_UNORM));
        m_resourceContext->addImageWithView("default-normal", createTexture(m_renderer, 1, 1, { 127, 127, 255, 255 }, VK_FORMAT_R8G8B8A8_UNORM));
        m_resourceContext->addImageWithView("default-ao", createTexture(m_renderer, 1, 1, { 255, 255, 255, 255 }, VK_FORMAT_R8G8B8A8_UNORM));
        m_resourceContext->createPipeline("pbrTex", "PbrTex.lua", m_renderGraph->getRenderPass(MainPass), 0);

        // Environment map
        LuaConfig config(m_renderer->getResourcesPath() / "Scripts/scene.lua");
        auto hdrName = config.get<std::string>("environmentMap").value_or("GreenwichPark") + ".hdr";
        auto envRefMap = createEnvironmentMap(m_renderer, hdrName, VK_FORMAT_R32G32B32A32_SFLOAT, true);
        std::shared_ptr<VulkanImageView> envRefMapView = envRefMap->createView(VK_IMAGE_VIEW_TYPE_2D);

        auto [cubeMap, cubeMapView] = convertEquirectToCubeMap(m_renderer, envRefMapView, 1024);

        auto [diffEnv, diffEnvView] = setupDiffuseEnvMap(m_renderer, *cubeMapView, 64);
        m_resourceContext->addImageWithView("envIrrMap", std::move(diffEnv), std::move(diffEnvView));
        auto [reflEnv, reflEnvView] = setupReflectEnvMap(m_renderer, *cubeMapView, 512);
        m_resourceContext->addImageWithView("filteredMap", std::move(reflEnv), std::move(reflEnvView));

        m_resourceContext->addImageWithView("cubeMap", std::move(cubeMap), std::move(cubeMapView));
        m_resourceContext->addImageWithView("brdfLut", integrateBrdfLut(m_renderer));
    }

    Material* PbrScene::createPbrTexMaterial(const std::string& type, const std::string& tag)
    {
        auto material = m_resourceContext->createMaterial("pbrTex" + tag, "pbrTex");
        material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        material->writeDescriptor(0, 1, *m_resourceContext->getUniformBuffer("camera"));

        material->writeDescriptor(1, 0, *m_lightSystem->getCascadedDirectionalLightBuffer());
        material->writeDescriptor(1, 1, *m_renderGraph->getNode(CsmPass).renderPass, 0, m_resourceContext->getSampler("nearestNeighbor"));

        VulkanSampler* linearRepeatSampler = m_resourceContext->getSampler("linearRepeat");
        const std::vector<std::string> texNames = { "diffuse", "metallic", "roughness", "normal", "ao" };
        const std::vector<VkFormat> formats = { VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };
        for (uint32_t i = 0; i < texNames.size(); ++i)
        {
            const std::string& filename = "PbrMaterials/" + type + "/" + texNames[i] + ".png";
            const auto& path = m_renderer->getResourcesPath() / "Textures" / filename;
            if (std::filesystem::exists(path))
            {
                std::string key = tag + "-" + texNames[i];
                m_resourceContext->addImageWithView(key, createTexture(m_renderer, filename, formats[i], true));
                material->writeDescriptor(1, 2 + i, *m_resourceContext->getImageView(key), linearRepeatSampler);
            }
            else
            {
                spdlog::warn("Texture type {} is using default values for '{}'", type, texNames[i]);
                std::string key = "default-" + texNames[i];
                material->writeDescriptor(1, 2 + i, *m_resourceContext->getImageView(key), linearRepeatSampler);
            }
        }

        material->writeDescriptor(2, 0, *m_resourceContext->getImageView("envIrrMap"), m_resourceContext->getSampler("linearClamp"));
        material->writeDescriptor(2, 1, *m_resourceContext->getImageView("filteredMap"), m_resourceContext->getSampler("linearMipmap"));
        material->writeDescriptor(2, 2, *m_resourceContext->getImageView("brdfLut"), m_resourceContext->getSampler("linearClamp"));
        return material;
    }

    void PbrScene::createShaderballs()
    {
        const std::vector<std::string> materials =
        {
            "Floor",
            "Grass",
            "Limestone",
            "Mahogany",
            "MetalGrid",
            "MixedMoss",
            "OrnateBrass",
            "PirateGold",
            "RedBricks",
            "RustedIron",
            "SteelPlate",
            "Snowdrift",
            "Hexstone"
        };

        auto pbrPipeline = m_resourceContext->createPipeline("pbrUnif", "PbrUnif.lua", m_renderGraph->getRenderPass(MainPass), 0);
        auto pbrMaterial = m_resourceContext->createMaterial("pbrUnif", pbrPipeline);
        pbrMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        pbrMaterial->writeDescriptor(0, 1, *m_resourceContext->getUniformBuffer("camera"));

        m_uniformMaterialParams.albedo = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        m_uniformMaterialParams.metallic = 0.1f;
        m_uniformMaterialParams.roughness = 0.1f;
        m_resourceContext->addUniformBuffer("pbrUnifParams", std::make_unique<UniformBuffer>(m_renderer, m_uniformMaterialParams, BufferUpdatePolicy::PerFrame));
        pbrMaterial->writeDescriptor(0, 2, *m_resourceContext->getUniformBuffer("pbrUnifParams"));
        pbrMaterial->writeDescriptor(1, 0, *m_lightSystem->getCascadedDirectionalLightBuffer());
        pbrMaterial->writeDescriptor(1, 1, m_renderGraph->getRenderPass(CsmPass), 0, m_resourceContext->getSampler("nearestNeighbor"));
        pbrMaterial->writeDescriptor(2, 0, *m_resourceContext->getImageView("envIrrMap"), m_resourceContext->getSampler("linearClamp"));
        pbrMaterial->writeDescriptor(2, 1, *m_resourceContext->getImageView("filteredMap"), m_resourceContext->getSampler("linearMipmap"));
        pbrMaterial->writeDescriptor(2, 2, *m_resourceContext->getImageView("brdfLut"), m_resourceContext->getSampler("linearClamp"));


        std::vector<VertexAttributeDescriptor> shadowVertexFormat = { VertexAttribute::Position };
        std::vector<VertexAttributeDescriptor> pbrVertexFormat = { VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent };

        const TriangleMesh mesh(m_renderer->getResourcesPath() / "Meshes/ShaderBall_FWVN_PosX.obj", pbrVertexFormat);
        m_resourceContext->addGeometry("shaderBallPbr", std::make_unique<Geometry>(m_renderer, mesh, pbrVertexFormat));
        m_resourceContext->addGeometry("shaderBallShadow", std::make_unique<Geometry>(m_renderer, mesh, shadowVertexFormat));

        const int32_t columnCount = 4;
        const int32_t rowCount = 3;
        for (int32_t i = 0; i < rowCount; ++i)
        {
            for (int32_t j = 0; j < columnCount; ++j)
            {
                const int32_t linearIdx = i * columnCount + j;

                auto pbrShaderBall = createRenderNode("pbrShaderBall" + std::to_string(linearIdx), 3 + linearIdx);

                const glm::mat4 translation = glm::translate(glm::vec3(5.0f * j, 0.0f, 5.0f * i));
                pbrShaderBall->transformPack->M = translation * glm::scale(glm::vec3(0.025f));
                pbrShaderBall->geometry = m_resourceContext->getGeometry("shaderBallPbr");

                const std::string materialName = materials[linearIdx % materials.size()];
                pbrShaderBall->pass(MainPass).material = createPbrTexMaterial(materialName, "ShaderBall" + std::to_string(linearIdx));
                pbrShaderBall->pass(MainPass).setPushConstantView(m_shaderBallUVScale);

                for (uint32_t i = 0; i < CascadeCount; ++i)
                {
                    auto& subpass = pbrShaderBall->subpass(CsmPass, i);
                    subpass.geometry = m_resourceContext->getGeometry("shaderBallShadow");
                    subpass.material = m_resourceContext->getMaterial("cascadedShadowMap" + std::to_string(i));
                }
            }
        }

        m_renderer->getDevice()->flushDescriptorUpdates();
    }

    void PbrScene::createPlane()
    {
        std::vector<VertexAttributeDescriptor> pbrVertexFormat = { VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord, VertexAttribute::Tangent };

        m_resourceContext->addGeometry("floor", std::make_unique<Geometry>(m_renderer, createPlaneMesh(pbrVertexFormat, 200.0f)));

        auto floor = createRenderNode("floor", 0);
        floor->transformPack->M = glm::scale(glm::vec3(1.0, 1.0f, 1.0f));
        floor->geometry = m_resourceContext->getGeometry("floor");
        floor->pass(MainPass).material = createPbrTexMaterial("Hexstone", "floor");
        floor->pass(MainPass).setPushConstants(glm::vec2(100.0f));

        m_renderer->getDevice()->flushDescriptorUpdates();
    }

    void PbrScene::setupInput()
    {
        m_connectionHandlers.emplace_back(m_app->getWindow()->keyPressed.subscribe([this](Key key, int)
        {
            switch (key)
            {
            case Key::F5:
                m_resourceContext->recreatePipelines();
                break;
            }
        }));

        m_connectionHandlers.emplace_back(m_app->getWindow()->mouseWheelScrolled.subscribe([this](double delta)
        {
            const float fovY = std::max(5.0f, std::min(90.0f, m_cameraController->getCamera().getFovY() + std::copysignf(5.0f, -delta)));
            m_cameraController->getCamera().setFovY(fovY);
        }));
    }

    void PbrScene::createGui(gui::Form* form)
    {
        using namespace gui;
        std::unique_ptr<Panel> panel = std::make_unique<Panel>(form);

        panel->setId("shadowMappingPanel");
        panel->setPadding({ 20, 20 });
        panel->setPosition({ 20, 40 });
        panel->setVerticalSizingPolicy(SizingPolicy::WrapContent);
        panel->setHorizontalSizingPolicy(SizingPolicy::WrapContent);

        int y = 0;
        auto addLabeledSlider = [&](const std::string& labelText, double val, double minVal, double maxVal) {
            auto label = std::make_unique<Label>(form, labelText);
            label->setPosition({ 0, y });
            panel->addControl(std::move(label));
            y += 20;

            auto slider = std::make_unique<DoubleSlider>(form, minVal, maxVal);
            slider->setId(labelText + "Slider");
            slider->setAnchor(Anchor::TopCenter);
            slider->setOrigin(Origin::TopCenter);
            slider->setPosition({ 0, y });
            slider->setValue(val);
            slider->setIncrement(0.01f);

            DoubleSlider* sliderPtr = slider.get();
            panel->addControl(std::move(slider));
            y += 30;

            return sliderPtr;
        };
        addLabeledSlider("Roughness", m_uniformMaterialParams.roughness, 0.0, 1.0)->valueChanged.subscribe<&PbrScene::setRoughness>(this);
        addLabeledSlider("Metallic", m_uniformMaterialParams.metallic, 0.0, 1.0)->valueChanged.subscribe<&PbrScene::setMetallic>(this);
        addLabeledSlider("Red", m_uniformMaterialParams.albedo.r, 0.0, 1.0)->valueChanged.subscribe<&PbrScene::setRedAlbedo>(this);
        addLabeledSlider("Green", m_uniformMaterialParams.albedo.g, 0.0, 1.0)->valueChanged.subscribe<&PbrScene::setGreenAlbedo>(this);
        addLabeledSlider("Blue", m_uniformMaterialParams.albedo.b, 0.0, 1.0)->valueChanged.subscribe<&PbrScene::setBlueAlbedo>(this);
        addLabeledSlider("U scale", m_shaderBallUVScale.s, 1.0, 20.0)->valueChanged.subscribe<&PbrScene::setUScale>(this);
        addLabeledSlider("V scale", m_shaderBallUVScale.t, 1.0, 20.0)->valueChanged.subscribe<&PbrScene::setVScale>(this);

        std::vector<std::string> materials;
        for (auto dir : std::filesystem::directory_iterator(m_renderer->getResourcesPath() / "Textures/PbrMaterials"))
            materials.push_back(dir.path().stem().string());
        materials.push_back("Uniform");

        auto comboBox = std::make_unique<gui::ComboBox>(form);
        comboBox->setId("materialComboBox");
        comboBox->setPosition({ 0, y });
        comboBox->setItems(materials);
        comboBox->itemSelected.subscribe<&PbrScene::onMaterialSelected>(this);
        panel->addControl(std::move(comboBox));
        y += 40;

        auto floorCheckBox = std::make_unique<gui::CheckBox>(form);
        floorCheckBox->setChecked(true);
        floorCheckBox->setText("Show Floor");
        floorCheckBox->setPosition({ 0, y });
        panel->addControl(std::move(floorCheckBox));

        form->add(std::move(panel));
    }
}