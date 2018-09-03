#include "ShapeFactory.hpp"

#include "Shape.hpp"
#include "Mesh.hpp"
#include "Sphere.hpp"

namespace crisp
{
    std::unique_ptr<Shape> ShapeFactory::create(std::string type, VariantMap parameters)
    {
        if (type == "mesh")
        {
            return std::make_unique<Mesh>(parameters);
        }
        else if (type == "sphere")
        {
            return std::make_unique<Sphere>(parameters);
        }
        else
        {
            std::cerr << "Unknown shape type \"" << type << "\" requested! Creating default shape type" << std::endl;
            return std::make_unique<Mesh>(parameters);
        }
    }
}