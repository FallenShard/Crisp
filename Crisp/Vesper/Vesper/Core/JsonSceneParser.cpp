#include "JsonSceneParser.hpp"

#include "Scene.hpp"

#include <fstream>
#include <iostream>

#include <json/json.hpp>

#include "Core/VariantMap.hpp"

#include "Samplers/SamplerFactory.hpp"
#include "Cameras/CameraFactory.hpp"
#include "Integrators/IntegratorFactory.hpp"
#include "Shapes/ShapeFactory.hpp"
#include "Lights/LightFactory.hpp"
#include "BSDFs/BSDFFactory.hpp"

namespace vesper
{
    using json = nlohmann::json;

    namespace
    {
        std::string fileToString(const std::string& fileName)
        {
            std::ifstream inputFile(fileName);
            std::string source;

            if (inputFile.is_open())
            {
                std::string line;
                while (std::getline(inputFile, line))
                    source += line + '\n';

                inputFile.close();
            }
            else
                std::cerr << "Could not open file " << fileName << std::endl;

            return source;
        }

        glm::vec3 parseVec3(const nlohmann::basic_json<>& jsonArray)
        {
            glm::vec3 result;
            for (int i = 0; i < std::min(static_cast<size_t>(result.length()), jsonArray.size()); i++)
                if (jsonArray[i].is_number())
                    result[i] = jsonArray[i];
            
            return result;
        }

        glm::ivec2 parseIvec2(const nlohmann::basic_json<>& jsonArray)
        {
            glm::ivec2 result;
            for (int i = 0; i < std::min(static_cast<size_t>(result.length()), jsonArray.size()); i++)
                if (jsonArray[i].is_number_integer())
                    result[i] = jsonArray[i];

            return result;
        }

        void parseParameters(VariantMap& params, nlohmann::basic_json<>& json)
        {
            if (json.is_null() || !json.is_object())
                return;

            for (auto it = json.begin(); it != json.end(); ++it)
            {
                if (it.value().is_boolean())
                {
                    params.insert(it.key(), it.value().get<bool>());
                }
                else if (it.value().is_number_integer())
                {
                    params.insert(it.key(), it.value().get<int>());
                }
                else if (it.value().is_number_float())
                {
                    params.insert(it.key(), it.value().get<float>());
                }
                else if (it.value().is_string())
                {
                    params.insert(it.key(), it.value().get<std::string>());
                }
                else if (it.value().is_array())
                {
                    auto& jsonArray = it.value();
                    auto arraySize = jsonArray.size();

                    if (arraySize > 0)
                    {
                        if (jsonArray[0].is_number_integer() && jsonArray.size() == 2)
                            params.insert(it.key(), parseIvec2(jsonArray));
                        else if (jsonArray[0].is_number())
                            params.insert(it.key(), parseVec3(jsonArray));

                        if (jsonArray[0].is_object())
                        {
                            Transform transform;
                            for (auto& item : jsonArray)
                            {
                                if (!item["scale"].is_null())
                                {
                                    glm::vec3 vec = parseVec3(item["scale"]);
                                    transform = Transform::createScale(vec) * transform;
                                }
                                else if (!item["translate"].is_null())
                                {
                                    glm::vec3 vec = parseVec3(item["translate"]);
                                    transform = Transform::createTranslation(vec) * transform;
                                }
                                else if (!item["rotate"].is_null())
                                {
                                    float angle = item["rotate"]["angle"];
                                    glm::vec3 vec = parseVec3(item["rotate"]["axis"]);
                                    transform = Transform::createRotation(angle, vec)* transform;
                                }
                            }
                            params.insert(it.key(), transform);
                        }
                    }
                }
            }
        }

        template <typename Type, typename FactoryType>
        std::unique_ptr<Type> create(nlohmann::basic_json<>& json)
        {
            std::string type = "default";
            VariantMap params;
            if (!json.is_null())
            {
                parseParameters(params, json["params"]);
                if (!json["type"].is_null() && json["type"].is_string())
                    type = json["type"];
            }
            return FactoryType::create(type, params);
        }
    }

    std::unique_ptr<Scene> JsonSceneParser::parse(const std::string& filePath)
    {
        auto jsonString = fileToString(filePath);
        json topLevelJson = json::parse(jsonString.c_str());

        auto sceneJson = topLevelJson["scene"];
        if (sceneJson.is_null())
        {
            std::cerr << "Specified json file has no \"scene\" key" << std::endl;
            return nullptr;
        }

        auto scene = std::make_unique<Scene>();

        scene->setIntegrator(std::move(create<Integrator, IntegratorFactory>(sceneJson["integrator"])));
        scene->setSampler(std::move(create<Sampler, SamplerFactory>(sceneJson["sampler"])));
        scene->setCamera(std::move(create<Camera, CameraFactory>(sceneJson["camera"])));
        
        auto shapes = sceneJson["shapes"];
        for (auto& shapeJson : shapes)
        {
            std::unique_ptr<Light> light = nullptr;
            if (!shapeJson["light"].is_null())
                light = create<Light, LightFactory>(shapeJson["light"]);

            std::unique_ptr<BSDF> bsdf = create<BSDF, BSDFFactory>(shapeJson["bsdf"]);
                
            scene->addShape(std::move(create<Shape, ShapeFactory>(shapeJson)), bsdf.get(), light.get());
            if (light) scene->addLight(std::move(light));
            scene->addBSDF(std::move(bsdf));
        }

        auto lights = sceneJson["lights"];
        for (auto& lightJson : lights)
            scene->addLight(std::move(create<Light, LightFactory>(lightJson)));
            

        scene->finishInitialization();
        return std::move(scene);
    }
}