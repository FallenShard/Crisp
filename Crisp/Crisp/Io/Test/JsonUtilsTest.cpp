#include <Crisp/Io/JsonUtils.hpp>

#include <Crisp/Core/UniqueTemporaryFile.hpp>

#include <gmock/gmock.h>

namespace crisp::test {
namespace {

TEST(JsonUtilsTest, HasField) {
    nlohmann::json json{};
    json["key"] = 123;
    EXPECT_TRUE(hasField<JsonType::NumberInt>(json, "key"));
}

TEST(JsonUtilsTest, LoadJsonFromFileReturnsErrorForInvalidJson) {
    UniqueTemporaryFile file("json");
    stringToFile(file.getPath(), "{ invalid json").unwrap();

    const auto result = loadJsonFromFile(file.getPath());

    EXPECT_FALSE(result.hasValue());
}
} // namespace
} // namespace crisp::test
