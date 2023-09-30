#pragma once

#include <memory>
#include <string>

namespace crisp {
namespace pt {
class Scene;
}

class XmlSceneParser {
public:
    std::unique_ptr<pt::Scene> parse(const std::string& fileName);
};
} // namespace crisp