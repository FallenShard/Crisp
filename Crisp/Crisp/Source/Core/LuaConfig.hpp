#pragma once

#include <optional>
#include <filesystem>

#include <lua/lua.hpp>
#include <CrispCore/Log.hpp>

namespace crisp
{
    namespace detail
    {
        template <class T>
        struct is_vector
        {
            static bool const value = false;
        };

        template <class T>
        struct is_vector<std::vector<T>>
        {
            static bool const value = true;
        };

        class ScopedVariable
        {
        public:
            ScopedVariable(lua_State* L, std::string_view name, int tableIndex)
                : m_L(L)
            {
                lua_getfield(m_L, tableIndex, name.data());
            }

            ScopedVariable(const ScopedVariable&) = delete;
            ScopedVariable operator=(const ScopedVariable&) = delete;

            ~ScopedVariable()
            {
                lua_pop(m_L, 1);
            }

            template <typename T>
            std::optional<T> convertTo()
            {
                if constexpr (std::is_arithmetic_v<T>)
                {
                    if (lua_type(m_L, -1) == LUA_TNUMBER)
                        return static_cast<T>(lua_tonumber(m_L, -1));
                }
                else if constexpr (std::is_convertible_v<T, std::string>)
                {
                    if (lua_type(m_L, -1) == LUA_TSTRING)
                        return std::string(lua_tostring(m_L, -1));
                }
                else if constexpr (is_vector<T>::value)
                {
                    using ElementType = typename T::value_type;
                    std::size_t numElements = lua_objlen(m_L, -1);
                    T vec;
                    vec.reserve(numElements);

                    for (std::size_t i = 0; i < numElements; ++i)
                    {
                        lua_pushnumber(m_L, i + 1);
                        lua_gettable(m_L, -2);
                        vec.push_back(convertTo<ElementType>().value_or(ElementType{}));

                        lua_pop(m_L, 1);
                    }

                    return vec;
                }

                return {};
            }

        protected:
            lua_State* m_L;
        };
    }

    class LuaTable : public detail::ScopedVariable
    {
    public:
        LuaTable(lua_State* L, std::string_view tableName, int tableIndex)
            : ScopedVariable(L, tableName, tableIndex)
        {
            if (!lua_istable(m_L, -1))
                logWarning("Table '{}' is undefined\n", tableName);
        }

        LuaTable(const LuaTable&) = delete;
        LuaTable operator=(const LuaTable&) = delete;

        template <typename T>
        std::optional<T> get(std::string_view variableName)
        {
            return detail::ScopedVariable(m_L, variableName, -1).convertTo<T>();
        }

        LuaTable operator[](std::string_view tableName)
        {
            return LuaTable(m_L, tableName, -1);
        }
    };

    class LuaConfig
    {
    public:
        LuaConfig()
            : m_L(lua_open())
        {
            luaL_openlibs(m_L);
        }

        LuaConfig(const std::filesystem::path& configPath)
            : m_L(lua_open())
        {
            luaL_openlibs(m_L);
            openFile(configPath);
        }

        ~LuaConfig()
        {
            lua_gc(m_L, LUA_GCCOLLECT, 0);
            lua_close(m_L);
        }

        void openFile(const std::filesystem::path& configPath)
        {
            int code = luaL_dofile(m_L, configPath.string().c_str());
            if (code != 0)
                logError("Error: {}\n", lua_tostring(m_L, -1));
        }

        template <typename T>
        std::optional<T> get(std::string_view variableName)
        {
            return detail::ScopedVariable(m_L, variableName, LUA_GLOBALSINDEX).convertTo<T>();
        }

        LuaTable operator[](std::string_view varName)
        {
            return LuaTable(m_L, varName, LUA_GLOBALSINDEX);
        }

        void printStackSize() const
        {
            logInfo("Stack size: {}\n", lua_gettop(m_L));
        }

    private:
        lua_State* m_L;
    };
}
