#pragma once

#include <optional>
#include <filesystem>

#include <lua/lua.hpp>
#include <spdlog/spdlog.h>

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

        template <class T>
        inline constexpr bool is_vector_v = is_vector<T>::value;

        class ScopedVariable
        {
        public:
            ScopedVariable(lua_State* L, std::string_view name, int tableIndex)
                : m_L(L)
            {
                lua_getfield(m_L, tableIndex, name.data());
            }

            ScopedVariable(lua_State* L, int itemIndex)
                : m_L(L)
            {
                lua_pushnumber(m_L, itemIndex + 1);
                lua_gettable(m_L, -2);
            }

            ScopedVariable(const ScopedVariable&) = delete;
            ScopedVariable operator=(const ScopedVariable&) = delete;

            ~ScopedVariable()
            {
                lua_pop(m_L, 1);
            }

            bool exists() const
            {
                return lua_type(m_L, -1) != LUA_TNIL;
            }

            template <typename T>
            std::optional<T> convertTo() const
            {
                if constexpr (std::is_arithmetic_v<T>)
                {
                    if (lua_type(m_L, -1) == LUA_TNUMBER)
                        return static_cast<T>(lua_tonumber(m_L, -1));
                    else if (lua_type(m_L, -1) == LUA_TBOOLEAN)
                        return static_cast<T>(lua_toboolean(m_L, -1));
                }
                else if constexpr (std::is_convertible_v<T, std::string>)
                {
                    if (lua_type(m_L, -1) == LUA_TSTRING)
                        return std::string(lua_tostring(m_L, -1));
                }
                else if constexpr (is_vector_v<T>)
                {
                    if (lua_type(m_L, -1) == LUA_TSTRING)
                        return {};

                    using ElementType = typename T::value_type;
                    std::size_t numElements = lua_objlen(m_L, -1);
                    T vec;
                    vec.reserve(numElements);

                    for (std::size_t i = 0; i < numElements; ++i)
                    {
                        lua_pushnumber(m_L, static_cast<lua_Number>(i + 1));
                        lua_gettable(m_L, -2);
                        vec.push_back(convertTo<ElementType>().value_or(ElementType{}));

                        lua_pop(m_L, 1);
                    }

                    return vec;
                }
                else
                    return {};

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
                spdlog::warn("Table '{}' is undefined\n", tableName);
        }

        LuaTable(lua_State* L, int itemIndex)
            : ScopedVariable(L, itemIndex)
        {
            /*if (!lua_istable(m_L, -1))
                logWarning("Array/table is undefined at index {}\n", itemIndex);*/
        }

        LuaTable(const LuaTable&) = delete;
        LuaTable operator=(const LuaTable&) = delete;

        template <typename T>
        std::optional<T> get(std::string_view variableName) const
        {
            return detail::ScopedVariable(m_L, variableName, -1).convertTo<T>();
        }

        LuaTable operator[](std::string_view tableName) const
        {
            return LuaTable(m_L, tableName, -1);
        }

        LuaTable operator[](int itemIndex) const
        {
            return LuaTable(m_L, itemIndex);
        }

        std::size_t getLength() const
        {
            return lua_objlen(m_L, -1);
        }
    };

    class LuaConfig
    {
    public:
        LuaConfig()
            : m_L(lua_open())
        {
            // Loads the standard Lua Libs into the module
            // Essentially allows use of Lua library in the loaded scripts
            luaL_openlibs(m_L);
        }

        LuaConfig(const std::filesystem::path& configPath)
            : LuaConfig()
        {
            openFile(configPath);
        }

        ~LuaConfig()
        {
            lua_gc(m_L, LUA_GCCOLLECT, 0);
            lua_close(m_L);
        }

        void openFile(const std::filesystem::path& configPath)
        {
            m_scriptPath = configPath;
            const int code = luaL_dofile(m_L, m_scriptPath.string().c_str());
            if (code != 0)
                spdlog::error("Error loading the script from a file: {}\n", lua_tostring(m_L, -1));
        }

        void openScript(const std::string_view script)
        {
            const std::string str(script);
            const int code = luaL_dostring(m_L, str.c_str());
            if (code != 0)
                spdlog::error("Error processing the script: {}\n", lua_tostring(m_L, -1));
        }

        template <typename T>
        std::optional<T> get(std::string_view variableName) const
        {
            return detail::ScopedVariable(m_L, variableName, LUA_GLOBALSINDEX).convertTo<T>();
        }

        LuaTable operator[](std::string_view varName)
        {
            return LuaTable(m_L, varName, LUA_GLOBALSINDEX);
        }

        void printStackSize() const
        {
            spdlog::info("Stack size: {}\n", lua_gettop(m_L));
        }

        bool hasVariable(std::string_view varName)
        {
            return detail::ScopedVariable(m_L, varName, LUA_GLOBALSINDEX).exists();
        }

    private:
        // The Lua state for the currently loaded script
        lua_State* m_L;

        // The path to the current script, if any
        // If the script is created from a string, the path will be empty
        std::filesystem::path m_scriptPath;
    };
}
