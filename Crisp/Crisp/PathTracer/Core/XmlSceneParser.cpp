#include <Crisp/PathTracer/Core/XmlSceneParser.hpp>

#include <Crisp/IO/FileUtils.hpp>
#include <Crisp/PathTracer/BSDFs/BSDFFactory.hpp>
#include <Crisp/PathTracer/BSSRDFs/BSSRDFFactory.hpp>
#include <Crisp/PathTracer/Cameras/CameraFactory.hpp>
#include <Crisp/PathTracer/Core/Scene.hpp>
#include <Crisp/PathTracer/Core/VariantMap.hpp>
#include <Crisp/PathTracer/Integrators/IntegratorFactory.hpp>
#include <Crisp/PathTracer/Lights/LightFactory.hpp>
#include <Crisp/PathTracer/Samplers/SamplerFactory.hpp>
#include <Crisp/PathTracer/Shapes/ShapeFactory.hpp>
#include <Crisp/PathTracer/Textures/TextureFactory.hpp>

#pragma warning(push)
#pragma warning(disable : 26495) // initialization
#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_iterators.hpp>
#include <rapidxml/rapidxml_print.hpp>
#include <rapidxml/rapidxml_utils.hpp>
#pragma warning(pop)

#include <fstream>
#include <iostream>
#include <sstream>

namespace crisp {
using namespace rapidxml;

namespace {
std::string replaceAll(std::string str, const std::string& pattern, const std::string& replacement) {
    size_t startPos = str.find(pattern, 0);
    while (startPos != std::string::npos) {
        str.replace(startPos, pattern.length(), replacement);
        startPos += replacement.length();
        startPos = str.find(pattern, startPos);
    }

    return str;
}

template <typename T>
T parse(const std::string& string);

template <>
bool parse<bool>(const std::string& string) {
    return string == "false" || string == "0" ? false : true;
}

template <>
int parse<int>(const std::string& string) {
    return std::atoi(string.c_str());
}

template <>
float parse<float>(const std::string& string) {
    return static_cast<float>(std::atof(string.c_str()));
}

template <>
std::string parse<std::string>(const std::string& string) {
    return string;
}

template <>
glm::vec2 parse<glm::vec2>(const std::string& string) {
    glm::vec2 result(0.0f);
    std::stringstream stringStream(replaceAll(string, ",", " "));
    std::string value;
    stringStream >> value;
    result.x = static_cast<float>(std::atof(value.c_str()));
    stringStream >> value;
    result.y = static_cast<float>(std::atof(value.c_str()));

    return result;
}

template <>
glm::vec3 parse<glm::vec3>(const std::string& string) {
    glm::vec3 result(0.0f);
    std::stringstream stringStream(replaceAll(string, ",", " "));
    std::string value;
    stringStream >> value;
    result.x = static_cast<float>(std::atof(value.c_str()));
    stringStream >> value;
    result.y = static_cast<float>(std::atof(value.c_str()));
    stringStream >> value;
    result.z = static_cast<float>(std::atof(value.c_str()));

    return result;
}

template <>
Spectrum parse<Spectrum>(const std::string& string) {
    Spectrum spectrum(0.0f);
    std::stringstream stringStream(replaceAll(string, ",", " "));
    std::string value;
    stringStream >> value;
    spectrum.r = static_cast<float>(std::atof(value.c_str()));
    stringStream >> value;
    spectrum.g = static_cast<float>(std::atof(value.c_str()));
    stringStream >> value;
    spectrum.b = static_cast<float>(std::atof(value.c_str()));

    return spectrum;
}

template <>
glm::ivec2 parse<glm::ivec2>(const std::string& string) {
    glm::ivec2 result(0);
    std::stringstream stringStream(replaceAll(string, ",", " "));
    std::string value;
    stringStream >> value;
    result.x = std::atoi(value.c_str());
    stringStream >> value;
    result.y = std::atoi(value.c_str());

    return result;
}

template <typename T>
inline VariantMap::VariantType parse(xml_node<char>* node) {
    return VariantMap::VariantType(parse<T>(std::string(node->first_attribute("value")->value())));
}

template <>
inline VariantMap::VariantType parse<Transform>(xml_node<char>* node) {
    Transform transform;
    for (auto child = node->first_node(); child != nullptr; child = child->next_sibling()) {
        std::string name = child->name();
        if (name == "translate") {
            glm::vec3 vec = parse<glm::vec3>(child->first_attribute("value")->value());
            transform = Transform::createTranslation(vec) * transform;
        } else if (name == "scale") {
            glm::vec3 scale = parse<glm::vec3>(child->first_attribute("value")->value());
            transform = Transform::createScale(scale) * transform;
        } else if (name == "rotate") {
            glm::vec3 axis = parse<glm::vec3>(child->first_attribute("axis")->value());
            float angle = parse<float>(child->first_attribute("angle")->value());
            transform = Transform::createRotation(angle, axis) * transform;
        }
    }
    return VariantMap::VariantType(transform);
}

void parseParameters(VariantMap& params, xml_node<char>* node) {
    static std::unordered_map<std::string, std::function<VariantMap::VariantType(xml_node<char>*)>> keyValueParser = {
        {"bool", [](xml_node<char>* node) { return parse<bool>(node); }},
        {"int", [](xml_node<char>* node) { return parse<int>(node); }},
        {"float", [](xml_node<char>* node) { return parse<float>(node); }},
        {"string", [](xml_node<char>* node) { return parse<std::string>(node); }},
        {"vec3", [](xml_node<char>* node) { return parse<glm::vec3>(node); }},
        {"vec2", [](xml_node<char>* node) { return parse<glm::vec2>(node); }},
        {"ivec2", [](xml_node<char>* node) { return parse<glm::ivec2>(node); }},
        {"spectrum", [](xml_node<char>* node) { return parse<Spectrum>(node); }},
        {"transform", [](xml_node<char>* node) { return parse<Transform>(node); }}};

    if (node == nullptr) {
        return;
    }

    for (auto child = node->first_node(); child != nullptr; child = child->next_sibling()) {
        std::string paramType = child->name();

        // Check if the node is a key value type
        auto itemIter = keyValueParser.find(paramType);
        if (itemIter == keyValueParser.end()) {
            continue;
        }

        // Extract the name
        auto nameAttrib = child->first_attribute("name");
        if (nameAttrib == nullptr) {
            std::cerr << "Parameter specified without a name!" << std::endl;
            continue;
        }
        std::string paramName = nameAttrib->value();

        // Check if value is present (unless "transform" type)
        auto valueAttrib = child->first_attribute("value");
        if (paramType != "transform" && valueAttrib == nullptr) {
            std::cerr << "Parameter " + paramName + " specified without a value!" << std::endl;
            continue;
        }

        params.insert(paramName, itemIter->second(child));
    }
}

template <typename Type, typename FactoryType>
std::unique_ptr<Type> create(xml_node<char>* node) {
    std::string type = "default";
    VariantMap params;
    if (node != nullptr) {
        parseParameters(params, node);

        auto typeAttrib = node->first_attribute("type");
        if (typeAttrib != nullptr) {
            type = typeAttrib->value();
        }
    }
    return FactoryType::create(type, params);
}

template <typename DataType>
std::unique_ptr<Texture<DataType>> create(xml_node<char>* node) {
    std::string type = "default";
    std::string dataType = "float";
    VariantMap params;
    if (node != nullptr) {
        parseParameters(params, node);

        auto typeAttrib = node->first_attribute("type");
        if (typeAttrib != nullptr) {
            type = typeAttrib->value();
        }
    }

    return TextureFactory::create<DataType>(type, params);
}
} // namespace

std::unique_ptr<pt::Scene> XmlSceneParser::parse(const std::string& filePath) {
    file<> xmlFile(filePath.c_str());
    xml_document<> document;
    document.parse<0>(xmlFile.data());

    auto sceneNode = document.first_node("scene");
    if (sceneNode == nullptr) {
        std::cerr << "Specified xml file has no \"scene\" node!" << std::endl;
        return nullptr;
    }

    auto scene = std::make_unique<pt::Scene>();

    scene->setIntegrator(std::move(create<Integrator, IntegratorFactory>(sceneNode->first_node("integrator"))));
    scene->setSampler(std::move(create<Sampler, SamplerFactory>(sceneNode->first_node("sampler"))));
    scene->setCamera(std::move(create<Camera, CameraFactory>(sceneNode->first_node("camera"))));

    auto shapes = sceneNode->first_node("shapes");
    if (shapes != nullptr) {
        for (auto shapeNode = shapes->first_node("shape"); shapeNode != nullptr;
             shapeNode = shapeNode->next_sibling("shape")) {
            std::unique_ptr<Light> light = nullptr;
            if (shapeNode->first_node("light") != nullptr) {
                light = create<Light, LightFactory>(shapeNode->first_node("light"));
            }

            std::unique_ptr<BSSRDF> bssrdf = nullptr;
            if (shapeNode->first_node("bssrdf") != nullptr) {
                bssrdf = create<BSSRDF, BSSRDFFactory>(shapeNode->first_node("bssrdf"));
            }

            std::unique_ptr<BSDF> bsdf = create<BSDF, BSDFFactory>(shapeNode->first_node("bsdf"));

            auto texNode =
                shapeNode->first_node("bsdf") ? shapeNode->first_node("bsdf")->first_node("texture") : nullptr;
            if (texNode != nullptr) {
                std::string dataType = "spectrum";
                if (texNode->first_attribute("data") != nullptr) {
                    dataType = texNode->first_attribute("data")->value();
                }

                if (dataType == "float") {
                    bsdf->setTexture(create<float>(shapeNode->first_node("bsdf")->first_node("texture")));
                } else {
                    bsdf->setTexture(create<Spectrum>(shapeNode->first_node("bsdf")->first_node("texture")));
                }
            }

            auto shape = create<Shape, ShapeFactory>(shapeNode);
            shape->setBSSRDF(std::move(bssrdf));

            scene->addShape(std::move(shape), bsdf.get(), light.get());

            if (light) {
                scene->addLight(std::move(light));
            }
            scene->addBSDF(std::move(bsdf));
        }
    }

    auto lights = sceneNode->first_node("lights");
    if (lights != nullptr) {
        for (auto lightNode = lights->first_node("light"); lightNode != nullptr;
             lightNode = lightNode->next_sibling("light")) {
            if (!strcmp(lightNode->first_attribute("type")->value(), "environment")) {
                scene->addEnvironmentLight(std::move(create<Light, LightFactory>(lightNode)));
            } else {
                scene->addLight(std::move(create<Light, LightFactory>(lightNode)));
            }
        }
    }

    scene->finishInitialization();

    return std::move(scene);
}
} // namespace crisp