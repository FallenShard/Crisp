#pragma once

#include <filesystem>
#include <string>

struct lua_State;

namespace crisp
{
    class LuaState
    {
    public:
        LuaState();
        ~LuaState();

        void runScript(const std::string& scriptFile);

    private:
        lua_State* m_state;
    };
}