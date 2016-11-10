#include "ShapeFactory.hpp"

#include "Shape.hpp"
#include "Mesh.hpp"

namespace vesper
{
    std::unique_ptr<Shape> ShapeFactory::create(std::string type, VariantMap parameters)
    {
        if (type == "mesh")
        {
            return std::make_unique<Mesh>(parameters);
        }
        else
        {
            std::cerr << "Unknown shape type \"" << type << "\" requested! Creating default shape type" << std::endl;
            return std::make_unique<Mesh>(parameters);
        }
    }
}