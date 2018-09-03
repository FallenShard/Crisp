#pragma once

#include <filesystem>

struct lua_State;

namespace crisp
{
    class LuaMachine
    {
    public:
        LuaMachine();
        ~LuaMachine();

        void runScript(const std::filesystem::path& scriptPath);

    private:
        lua_State* m_state;
    };
}