#pragma once

#include <iostream>
#include <map>

#include <mapbox/variant.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Spectrums/Spectrum.hpp"
#include "Math/Transform.hpp"

namespace vesper
{
    class VariantMap
    {
    public:
        template <typename T>
        void insert(const std::string& key, T value)
        {
            m_map.insert_or_assign(key, VariantType(value));
        }

        void remove(const std::string& key)
        {
            m_map.erase(key);
        }

        template <typename T>
        T get(const std::string& key, T defaultVal = T()) const
        {
            if (m_map.find(key) != m_map.end())
            {
                auto var = m_map.at(key);
                
                try
                {
                    T value = var.get<T>();
                    return value;
                }
                catch (mapbox::util::bad_variant_access ex)
                {
                    std::cerr << "Could not access variant variable with name: " + key << std::endl;
                }
            }

            return defaultVal;
        }

        bool contains(const std::string& key) const
        {
            return m_map.find(key) != m_map.end();
        }

    private:
        using VariantType = mapbox::util::variant
        <
            bool, 
            int,
            float,
            std::string,
            Spectrum,
            glm::vec2,
            glm::ivec2,
            glm::vec3,
            Transform
        >;

        std::map<std::string, VariantType> m_map;
    };
}