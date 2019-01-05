#pragma once

#include <optional>
#include <filesystem>

#include <lua/lua.hpp>
#include <CrispCore/Log.hpp>

namespace crisp
{
    namespace detail
    {
        template <typename T, typename = typename std::enable_if_t<std::is_arithmetic_v<T>>>
        inline std::optional<T> getLuaVariable(lua_State* L, const char* variableName)
        {
            lua_getglobal(L, variableName);

            std::optional<T> opt;
            if (lua_isnumber(L, -1))
                opt = static_cast<T>(lua_tonumber(L, -1));

            lua_pop(L, 1);
            return opt;
        }

        inline std::optional<std::string> getLuaVariable(lua_State* L, const char* variableName)
        {
            lua_getglobal(L, variableName);

            std::optional<std::string> opt;
            if (lua_isstring(L, -1))
                opt = lua_tostring(L, -1);

            lua_pop(L, 1);
            return opt;
        }
    }

    class LuaConfig
    {
    public:
        LuaConfig(const std::filesystem::path& configPath)
            : m_L(lua_open())
        {
            luaL_openlibs(m_L);

            int code = luaL_dofile(m_L, configPath.string().c_str());
            if (code != 0)
                logError("Error: {}\n", lua_tostring(m_L, -1));
        }

        ~LuaConfig()
        {
            lua_gc(m_L, LUA_GCCOLLECT, 0);
            lua_close(m_L);
        }

        template <typename T>
        std::optional<T> get(const char* variableName)
        {
            using detail::getLuaVariable;
            return getLuaVariable(m_L, variableName);
        }

        //std::unique_ptr<TableProxy> getTable(const char* variableName)
        //{
        //    lua_getglobal(L, variableName);
        //    if (!lua_istable(L, -1))
        //    {
        //        lua_pop(L, 1);
        //        return nullptr;
        //    }
        //
        //    return std::make_unique<TableProxy>(L);
        //}

    private:
        template <typename T>
        std::optional<T> getField(const char* key)
        {
            lua_pushstring(L, key);
            lua_gettable(L, -2);

            std::optional<T> opt;
            if (lua_isnumber(L, -1))
                opt = static_cast<T>(lua_tonumber(L, -1));

            lua_pop(L, 1);
            return opt;
        }

        lua_State* m_L;
    };
}
