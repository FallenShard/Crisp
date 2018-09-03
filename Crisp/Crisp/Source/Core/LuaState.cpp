#include "LuaState.hpp"

#include <lua/lua.hpp>
#include <CrispCore/Log.hpp>

namespace crisp
{
    namespace
    {
        std::filesystem::path ScriptFolder("../../Resources/Scripts/");

        int printVal(lua_State* state)
        {
            int numArgs = lua_gettop(state);

            for (int i = 1; i <= numArgs; i++)
            {
                lua_Number x = lua_tonumber(state, i);
                logDebug("I was called from Lua! Value: ", x);
            }

            return 0;
        }

        struct VesperPoint
        {
            int x;
            int y;

            VesperPoint(int x, int y) : x(x), y(y) {}

            void setX(int x) { this->x = x; }

            static const char className[];
        };

        const char VesperPoint::className[] = "VesperPoint";

        int push_object_metatable(lua_State* L);

        int destroyVesperPoint(lua_State* L)
        {
            delete *static_cast<VesperPoint**>(luaL_checkudata(L, 1, "VesperObject"));
            return 0;
        }

        int createVesperPoint(lua_State* L)
        {
            int numArgs = lua_gettop(L);
            if (numArgs != 2)
                return luaL_error(L, "Wrong number of arguments passed to VesperPoint()");

            luaL_checktype(L, 1, LUA_TTABLE);
            lua_newtable(L);

            lua_pushvalue(L, 1);
            lua_setmetatable(L, -2);

            lua_pushvalue(L, 1);
            lua_setfield(L, 1, "__index");

            VesperPoint** vp = (VesperPoint**)lua_newuserdata(L, sizeof(VesperPoint));

            const lua_Number x = luaL_checknumber(L, 2);
            const lua_Number y = luaL_checknumber(L, 3);
            *vp = new VesperPoint(x, y);

            luaL_getmetatable(L, "VesperPoint");
            lua_setmetatable(L, -2);

            lua_setfield(L, -2, "__self");

            return 1;
        }




        template <typename T>
        void registerType(lua_State* L)
        {
            lua_newtable(L);
            int methods = lua_gettop(L);

            luaL_newmetatable(L, T::className);
            int metatable = lua_gettop(L);

            lua_pushvalue(L, methods);
            set(L, metatable, "__metatable");

            lua_pushvalue(L, methods);
            set(L, metatable, "__index");

            lua_pushcfunction(L, T::toString);
            set(L, metatable, "__tostring");



        }

    }

    LuaState::LuaState()
        : m_state(luaL_newstate())
    {
        luaL_openlibs(m_state);

        luaL_Reg regs[] =
        {
            { "printVal", printVal },
            { nullptr, nullptr }
        };

        luaL_register(m_state, "printer", regs);

        lua_register(m_state, "VesperPoint", createVesperPoint);

        luaL_newmetatable(m_state, "VesperPoint");
        const luaL_Reg vpRegs[] =
        {
            { "new", createVesperPoint },
            { "__gc", destroyVesperPoint },
            { nullptr, nullptr }
        };

        //lua_pushcfunction(m_state, vpRegs[0]);

    }

    LuaState::~LuaState()
    {
        lua_close(m_state);
    }

    void LuaState::runScript(const std::string& scriptFile)
    {
        int status = luaL_dofile(m_state, (ScriptFolder / "callhost.lua").string().c_str());
        if (status != 0)
        {
            logError("Failed to load Lua script: ", lua_tostring(m_state, -1));
            return;
        }

        //lua_newtable(m_state);
        //
        //
        //for (int i = 1; i <= 5; i++) {
        //    lua_pushnumber(m_state, i);
        //    lua_pushnumber(m_state, i * 2);
        //    lua_rawset(m_state, -3);
        //}
        //
        //lua_setglobal(m_state, "foo");
        //
        //status = lua_pcall(m_state, 0, LUA_MULTRET, 0);
        //if (status != 0) {
        //    logError("Failed to run lua script: ", lua_tostring(m_state, -1));
        //    return;
        //}
        //
        //lua_Number sum = lua_tonumber(m_state, -1);
        //
        //logDebug("Sum: ", sum);
        //
        //lua_pop(m_state, 1);
    }
}