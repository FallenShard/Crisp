#include "XmlSceneParser.hpp"

#include "Scene.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_iterators.hpp>
#include <rapidxml/rapidxml_utils.hpp>
#include <rapidxml/rapidxml_print.hpp>

#include "Core/VariantMap.hpp"

#include "Samplers/SamplerFactory.hpp"
#include "Cameras/CameraFactory.hpp"
#include "Integrators/IntegratorFactory.hpp"
#include "Shapes/ShapeFactory.hpp"
#include "Lights/LightFactory.hpp"
#include "BSDFs/BSDFFactory.hpp"
#include "Textures/TextureFactory.hpp"

namespace vesper
{
    using namespace rapidxml;
    namespace
    {
        std::string replaceAll(std::string str, const std::string& pattern, const std::string& replacement)
        {
            size_t startPos = str.find(pattern, 0);
            while (startPos != std::string::npos)
            {
                str.replace(startPos, pattern.length(), replacement);
                startPos += replacement.length();
                startPos = str.find(pattern, startPos);
            }

            return str;
        }

        static std::vector<std::string> tokenize(const std::string& string, const std::string& delimiter)
        {
            std::vector<std::string> result;
            size_t start = 0;
            size_t end = 0;

            while (end != std::string::npos)
            {
                end = string.find(delimiter, start);

                result.push_back(string.substr(start, (end == std::string::npos) ? std::string::npos : end - start));

                start = ((end > (std::string::npos - delimiter.size())) ? std::string::npos : end + delimiter.size());
            }

            return result;
        }

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

        template<typename T>
        T parse(const std::string& string);

        template<>
        bool parse<bool>(const std::string& string)
        {
            return string == "false" || string == "0" ? false : true;
        }

        template<>
        int parse<int>(const std::string& string)
        {
            return std::atoi(string.c_str());
        }

        template<>
        float parse<float>(const std::string& string)
        {
            return static_cast<float>(std::atof(string.c_str()));
        }

        template<>
        glm::vec2 parse<glm::vec2>(const std::string& string)
        {
            glm::vec2 result(0.0f);
            std::stringstream stringStream(replaceAll(string, ",", " "));
            std::string value;
            stringStream >> value; result.x = static_cast<float>(std::atof(value.c_str()));
            stringStream >> value; result.y = static_cast<float>(std::atof(value.c_str()));

            return result;
        }

        template<>
        glm::vec3 parse<glm::vec3>(const std::string& string)
        {
            glm::vec3 result(0.0f);
            std::stringstream stringStream(replaceAll(string, ",", " "));
            std::string value;
            stringStream >> value; result.x = static_cast<float>(std::atof(value.c_str()));
            stringStream >> value; result.y = static_cast<float>(std::atof(value.c_str()));
            stringStream >> value; result.z = static_cast<float>(std::atof(value.c_str()));
            
            return result;
        }

        template<>
        Spectrum parse<Spectrum>(const std::string& string)
        {
            Spectrum spectrum(0.0f);
            std::stringstream stringStream(replaceAll(string, ",", " "));
            std::string value;
            stringStream >> value; spectrum.r = static_cast<float>(std::atof(value.c_str()));
            stringStream >> value; spectrum.g = static_cast<float>(std::atof(value.c_str()));
            stringStream >> value; spectrum.b = static_cast<float>(std::atof(value.c_str()));

            return spectrum;
        }

        template<>
        glm::ivec2 parse<glm::ivec2>(const std::string& string)
        {
            glm::ivec2 result(0);
            std::stringstream stringStream(replaceAll(string, ",", " "));
            std::string value;
            stringStream >> value; result.x = std::atoi(value.c_str());
            stringStream >> value; result.y = std::atoi(value.c_str());

            return result;
        }

        Transform parseTransform(xml_node<char>* node)
        {
            Transform transform;
            for (auto child = node->first_node(); child != nullptr; child = child->next_sibling())
            {
                std::string name = child->name();
                if (name == "translate")
                {
                    glm::vec3 vec = parse<glm::vec3>(child->first_attribute("value")->value());
                    transform = Transform::createTranslation(vec) * transform;
                }
                else if (name == "scale")
                {
                    glm::vec3 scale = parse<glm::vec3>(child->first_attribute("value")->value());
                    transform = Transform::createScale(scale) * transform;
                }
                else if (name == "rotate")
                {
                    glm::vec3 axis = parse<glm::vec3>(child->first_attribute("axis")->value());
                    float angle = parse<float>(child->first_attribute("angle")->value());
                    transform = Transform::createRotation(angle, axis) * transform;
                }
            }
            return transform;
        }

        void parseParameters(VariantMap& params, xml_node<char>* node)
        {
            if (node == nullptr)
                return;

            for (auto child = node->first_node(); child != nullptr; child = child->next_sibling())
            {
                std::string paramType = child->name();

                auto nameAttrib = child->first_attribute("name");
                if (nameAttrib == nullptr)
                {
                    std::cerr << "Parameter specified without a name!" << std::endl;
                    continue;
                }
                std::string paramName = nameAttrib->value();

                auto valueAttrib = child->first_attribute("value");
                if (paramType != "transform" && valueAttrib == nullptr)
                {
                    std::cerr << "Parameter " + paramName + " specified without a value!" << std::endl;
                    continue;
                }
                std::string paramValue = valueAttrib ? valueAttrib->value() : "";

                if (paramType == "bool")
                {
                    params.insert(paramName, parse<bool>(paramValue));
                }
                else if (paramType == "int")
                {
                    params.insert(paramName, parse<int>(paramValue));
                }
                else if (paramType == "float")
                {
                    params.insert(paramName, parse<float>(paramValue));
                }
                else if (paramType == "string")
                {
                    params.insert(paramName, paramValue);
                }
                else if (paramType == "vec2")
                {
                    params.insert(paramName, parse<glm::vec2>(paramValue));
                }
                else if (paramType == "vec3")
                {
                    params.insert(paramName, parse<glm::vec3>(paramValue));
                }
                else if (paramType == "ivec2")
                {
                    params.insert(paramName, parse<glm::ivec2>(paramValue));
                }
                else if (paramType == "spectrum")
                {
                    params.insert(paramName, parse<Spectrum>(paramValue));
                }
                else if (paramType == "transform")
                {
                    params.insert(paramName, parseTransform(child));
                }
                else
                {
                    std::cerr << paramName << " has specified unrecognized type!" << std::endl;
                }
            }
        }

        template <typename Type, typename FactoryType>
        std::unique_ptr<Type> create(xml_node<char>* node)
        {
            std::string type = "default";
            VariantMap params;
            if (node != nullptr)
            {
                parseParameters(params, node->first_node("parameters"));

                auto typeAttrib = node->first_attribute("type");
                if (typeAttrib != nullptr)
                {
                    type = typeAttrib->value();
                }
            }
            return FactoryType::create(type, params);
        }

        template <typename DataType>
        std::unique_ptr<Texture<DataType>> createTexture(xml_node<char>* node)
        {
            std::string type = "default";
            std::string dataType = "float";
            VariantMap params;
            if (node != nullptr)
            {
                parseParameters(params, node->first_node("parameters"));

                auto typeAttrib = node->first_attribute("type");
                if (typeAttrib != nullptr)
                    type = typeAttrib->value();

                
            }

            return TextureFactory::create<(type, params);
        }
    }

    std::unique_ptr<Scene> XmlSceneParser::parse(const std::string& filePath)
    {
        file<> xmlFile(filePath.c_str());
        xml_document<> document;
        document.parse<0>(xmlFile.data());

        auto sceneNode = document.first_node("scene");
        if (sceneNode == nullptr)
        {
            std::cerr << "Specified xml file has no \"scene\" node!" << std::endl;
            return nullptr;
        }

        auto scene = std::make_unique<Scene>();

        scene->setIntegrator(std::move(create<Integrator, IntegratorFactory>(sceneNode->first_node("integrator"))));
        scene->setSampler(std::move(create<Sampler, SamplerFactory>(sceneNode->first_node("sampler"))));
        scene->setCamera(std::move(create<Camera, CameraFactory>(sceneNode->first_node("camera"))));
        
        auto shapes = sceneNode->first_node("shapes");
        if (shapes != nullptr)
        {
            for (auto shapeNode = shapes->first_node("shape"); shapeNode != nullptr; shapeNode = shapeNode->next_sibling("shape"))
            {
                std::unique_ptr<Light> light = nullptr;
                if (shapeNode->first_node("light") != nullptr)
                {
                    light = create<Light, LightFactory>(shapeNode->first_node("light"));
                }


                std::unique_ptr<BSDF> bsdf = create<BSDF, BSDFFactory>(shapeNode->first_node("bsdf"));
                if (shapeNode->first_node("bsdf")->first_node("texture") != nullptr)
                {
                    std::string dataType = "float";
                    auto dataAttrib = shapeNode->first_node("bsdf")->first_node("texture")->first_attribute("data");
                    if (dataAttrib != nullptr)
                        dataType = dataAttrib->value();

                    if (dataType == "float")
                    {
                        bsdf->setTexture(create<Texture<float>, TextureFactory<float>>(shapeNode->first_node("bsdf")->first_node("texture")));
                    }
                    else if (dataType == "spectrum")
                    {
                        bsdf->setTexture(create<Texture<Spectrum>, TextureFactory<Spectrum>>(shapeNode->first_node("bsdf")->first_node("texture")));
                    }
                }

                scene->addShape(std::move(create<Shape, ShapeFactory>(shapeNode)), bsdf.get(), light.get());

                if (light)
                {
                    scene->addLight(std::move(light));
                }
                scene->addBSDF(std::move(bsdf));
            }
        }
        
        auto lights = sceneNode->first_node("lights");
        if (lights != nullptr)
        {
            for (auto lightNode = lights->first_node("light"); lightNode != nullptr; lightNode = lightNode->next_sibling("light"))
            {
                scene->addLight(std::move(create<Light, LightFactory>(lightNode)));
            }
        }
        
        scene->finishInitialization();
        return std::move(scene);
    }
}