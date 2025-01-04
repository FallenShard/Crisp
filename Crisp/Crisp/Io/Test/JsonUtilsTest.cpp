#include <Crisp/Io/JsonUtils.hpp>

#include <gmock/gmock.h>

namespace crisp::test {
namespace {

TEST(JsonUtilsTest, HasField) {
    nlohmann::json json{};
    json["key"] = 123;
    EXPECT_TRUE(hasField<JsonType::NumberInt>(json, "key"));
}
} // namespace
} // namespace crisp::test