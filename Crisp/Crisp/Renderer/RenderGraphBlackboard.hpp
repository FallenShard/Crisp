#pragma once

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/HashMap.hpp>

#include <any>
#include <typeindex>

namespace crisp
{

class RenderGraphBlackboard
{
public:
    RenderGraphBlackboard() = default;
    ~RenderGraphBlackboard() = default;

    RenderGraphBlackboard(const RenderGraphBlackboard&) = delete;
    RenderGraphBlackboard& operator=(const RenderGraphBlackboard&) = delete;

    RenderGraphBlackboard(RenderGraphBlackboard&&) = default;
    RenderGraphBlackboard& operator=(RenderGraphBlackboard&&) = default;

    template <typename T>
    T& insert()
    {
        const auto typeIdx{std::type_index(typeid(T))}; // NOLINT
        if (m_resourceGroups.find(typeIdx) != m_resourceGroups.end())
        {
            CRISP_FATAL("Blackboard '{}' already contains type '{}'.", m_name, typeid(T).name());
        }

        auto [iter, _] = m_resourceGroups.emplace(typeIdx, std::make_any<T>());
        return std::any_cast<T&>(iter->second);
    }

    template <typename T>
    const T& get() const
    {
        const auto typeIdx{std::type_index(typeid(T))}; // NOLINT
        return std::any_cast<const T&>(m_resourceGroups.at(typeIdx));
    }

    template <typename T>
    T& get()
    {
        const auto typeIdx{std::type_index(typeid(T))}; // NOLINT
        return std::any_cast<T&>(m_resourceGroups.at(typeIdx));
    }

    size_t size() const
    {
        return m_resourceGroups.size();
    }

private:
    std::string m_name;
    FlatHashMap<std::type_index, std::any> m_resourceGroups;
};

} // namespace crisp