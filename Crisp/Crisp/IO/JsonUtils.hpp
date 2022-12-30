#pragma once

#include <Crisp/IO/FileUtils.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>

namespace crisp
{

enum class JsonType
{
    Null,
    Object,
    Array,
    String,
    Boolean,
    NumberInt,
    NumberUint,
    NumberFloat,
    Binary
};

template <JsonType type>
constexpr nlohmann::detail::value_t toNlohmannJsonType()
{
    if constexpr (type == JsonType::Null)
        return nlohmann::detail::value_t::null;
    if constexpr (type == JsonType::Object)
        return nlohmann::detail::value_t::object;
    if constexpr (type == JsonType::Array)
        return nlohmann::detail::value_t::array;
    if constexpr (type == JsonType::String)
        return nlohmann::detail::value_t::string;
    if constexpr (type == JsonType::Boolean)
        return nlohmann::detail::value_t::boolean;
    if constexpr (type == JsonType::NumberInt)
        return nlohmann::detail::value_t::number_integer;
    if constexpr (type == JsonType::NumberUint)
        return nlohmann::detail::value_t::number_unsigned;
    if constexpr (type == JsonType::NumberFloat)
        return nlohmann::detail::value_t::number_float;
    if constexpr (type == JsonType::Binary)
        return nlohmann::detail::value_t::binary;
}

template <JsonType fieldType>
bool hasField(const nlohmann::json& json, const std::string_view key)
{
    return json.contains(key) && json[key].type() == toNlohmannJsonType<fieldType>();
}

inline Result<nlohmann::json> loadJsonFromFile(const std::filesystem::path& filePath)
{
    auto maybeString = fileToString(filePath);
    if (!maybeString)
    {
        return resultError("Failed to load json from a file: {}", maybeString.getError());
    }

    return nlohmann::json::parse(maybeString.extract());
}
} // namespace crisp
