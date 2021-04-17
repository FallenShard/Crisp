#include "LuaMachine.hpp"

#include <optional>
#include <array>
#include <string_view>

#include <lua/lua.hpp>

namespace crisp
{
    struct Point
    {
        int x;
        int y;

        Point() : x(10), y(20) {}
        Point(lua_State* L)
            : x(static_cast<int>(luaL_checknumber(L, 1)))
            , y(static_cast<int>(luaL_checknumber(L, 1)))
        {
        }

        int prod(lua_State* L)
        {
            lua_pushnumber(L, x * y);
            return 1;
        }

        int setX(lua_State* L)
        {
            x = static_cast<int>(luaL_checknumber(L, 1));
            return 0;
        }

        static constexpr const char* className = "Point";
    };

    template <typename T>
    int printAddress(lua_State* L)
    {
        char buff[32];
        T* obj = static_cast<T*>(lua_touserdata(L, 1));
        sprintf(buff, "%p", (void*)obj);
        lua_pushfstring(L, "%s (%s)", T::className, buff);

        return 1;
    }

    template <typename T>
    static int create(lua_State* L)
    {
        lua_remove(L, 1);   // use classname:new(), instead of classname.new()
        T* obj = new T(L);  // call constructor for T objects
        push(L, obj, true); // gc_T will delete this object
        return 1;           // userdata containing pointer to T object
    }

    //static void weaktable(lua_State* L, const char* mode)
    //{
    //    lua_newtable(L);
    //    lua_pushvalue(L, -1);  // table is its own metatable
    //    lua_setmetatable(L, -2);
    //    lua_pushliteral(L, "__mode");
    //    lua_pushstring(L, mode);
    //    lua_settable(L, -3);   // metatable.__mode = mode
    //}

    //static void subtable(lua_State* L, int tindex, const char* name, const char* mode)
    //{
    //    lua_pushstring(L, name);
    //    lua_gettable(L, tindex);
    //    if (lua_isnil(L, -1)) {
    //        lua_pop(L, 1);
    //        lua_checkstack(L, 3);
    //        weaktable(L, mode);
    //        lua_pushstring(L, name);
    //        lua_pushvalue(L, -2);
    //        lua_settable(L, tindex);
    //    }
    //}

    //static void *pushuserdata(lua_State* L, void* key, size_t sz) {
    //    void* userData = nullptr;
    //    lua_pushlightuserdata(L, key);
    //    lua_gettable(L, -2);     // lookup[key]
    //    if (lua_isnil(L, -1))
    //    {
    //        lua_pop(L, 1);         // drop nil
    //        lua_checkstack(L, 3);
    //        userData = lua_newuserdata(L, sz);  // create new userdata
    //        lua_pushlightuserdata(L, key);
    //        lua_pushvalue(L, -2);  // dup userdata
    //        lua_settable(L, -4);   // lookup[key] = userdata
    //    }
    //    return userData;
    //}

    template <typename T>
    static int push(lua_State *L, T* obj, bool gc = false)
    {
        if (!obj)
        {
            lua_pushnil(L);
            return 0;
        }

        luaL_getmetatable(L, T::className);  // lookup metatable in Lua registry
        if (lua_isnil(L, -1))
            luaL_error(L, "%s missing metatable", T::className);

        int mt = lua_gettop(L);
        subtable(L, mt, "userdata", "v");
        obj = static_cast<T*>(pushuserdata(L, obj, sizeof(T*)));
        if (obj) {
            lua_pushvalue(L, mt);
            lua_setmetatable(L, -2);
            if (gc == false) {
                lua_checkstack(L, 3);
                subtable(L, mt, "do not trash", "k");
                lua_pushvalue(L, -2);
                lua_pushboolean(L, 1);
                lua_settable(L, -3);
                lua_pop(L, 1);
            }
        }
        lua_replace(L, mt);
        lua_settop(L, mt);
        return mt;  // index of userdata containing pointer to T object
    }

    template <typename T>
    int destroy(lua_State* L)
    {
        if (luaL_getmetafield(L, 1, "do not trash")) {
            lua_pushvalue(L, 1);  // dup userdata
            lua_gettable(L, -2);
            if (!lua_isnil(L, -1)) return 0;  // do not delete object
        }
        T* obj = static_cast<T*>(lua_touserdata(L, 1));
        if (obj) delete obj;  // call destructor for T objects
        return 0;
    }

    template <typename T>
    T* check(lua_State* L, int narg) {
        T* obj = static_cast<T*>(luaL_checkudata(L, narg, T::className));

        if (!obj)
            luaL_typerror(L, narg, T::className);

        return obj;
    }

    //static void set(lua_State* L, int table_index, const char *key)
    //{
    //    lua_pushstring(L, key);
    //    lua_insert(L, -2);  // swap value and key
    //    lua_settable(L, table_index);
    //}

    template <typename T>
    using MemFnPtrType = int(T::*)(lua_State*);

    template <typename T>
    struct FuncRegType
    {
        const char* name;
        MemFnPtrType<T> memFn;
    };

    template <typename T>
    struct FuncRegistry {};

    template <>
    struct FuncRegistry<Point>
    {
        static constexpr FuncRegType<Point> entries[] = {
            { "prod", &Point::prod },
            { "setX", &Point::setX },
            { nullptr, nullptr }
        };
    };

    template <typename T>
    void registerType(lua_State* L)
    {
        lua_newtable(L);
        int methodsIdx = lua_gettop(L);

        luaL_newmetatable(L, T::className);
        int metatableIdx = lua_gettop(L);

        lua_pushvalue(L, methodsIdx);
        set(L, LUA_GLOBALSINDEX, T::className);

        lua_pushvalue(L, methodsIdx);
        set(L, metatableIdx, "__metatable");

        lua_pushvalue(L, methodsIdx);
        set(L, metatableIdx, "__index");

        lua_pushcfunction(L, printAddress<T>);
        set(L, metatableIdx, "__tostring");

        lua_pushcfunction(L, destroy<T>);
        set(L, metatableIdx, "__gc");

        lua_newtable(L);                // mt for method table
        lua_pushcfunction(L, create<T>);
        lua_pushvalue(L, -1);           // dup new_T function
        set(L, methodsIdx, "new");      // add new_T to method table
        set(L, -3, "__call");           // mt.__call = new_T
        lua_setmetatable(L, methodsIdx);

        for (const auto& regFun : FuncRegistry<T>::entries)
        {
            if (!regFun.name)
                break;

            lua_pushstring(L, regFun.name);
            lua_pushlightuserdata(L, (void*)&regFun);
            lua_pushcclosure(L, thunk<Point>, 1);
            lua_settable(L, methodsIdx);
        }

        lua_pop(L, 2);
    }

    // member function dispatcher
    template <typename T>
    static int thunk(lua_State* L)
    {
        // stack has userdata, followed by method args
        T* obj = check<T>(L, 1);  // get 'self', or if you prefer, 'this'
        lua_remove(L, 1);  // remove self so member function args start at index 1
        // get member function from upvalue
        FuncRegType<T>* l = static_cast<FuncRegType<T>*>(lua_touserdata(L, lua_upvalueindex(1)));
        return (obj->*(l->memFn))(L);  // call member function
    }

    std::string script = "function printf(...) io.write(string.format(unpack(arg))) end"
        "function Point : prod2()"
        "printf(\"Point megaprod = $%0.02f\n\", self:prod() * self:prod())"
        "end"
        "io.output(io.stdout)"
        "a = Point(100, 150)"
        "b = Point : new(123, 123)"

        "print('a =', a)"
        "print('b =', b)"
        "print('metatable =', getmetatable(a))"
        "print('Point =', Point)"
        "table.foreach(Point, print)"

        "a:prod2() a:setX(10) a:prod2() a:setX(20) a:prod()"

        "parent = {}"

        "function parent : rob(amount)"
        "amount = amount or self:prod()"
        "self:setX(1)"
        "return amount"
        "end"

        "getmetatable(Point).__index = parent"

        "debug.debug()";

    std::string tinyScript = "width = 200\nheight=300\nbackground = {r=0.30, g=0.10, b=0}";


    struct TableProxy
    {
        TableProxy(lua_State* L)
            : L(L)
        {
        }

        ~TableProxy()
        {
            lua_pop(L, 1);
        }

        template <typename T>
        std::optional<T> get(const char* key)
        {
            lua_pushstring(L, key);
            lua_gettable(L, -2);

            std::optional<T> opt;
            if (lua_isnumber(L, -1))
                opt = static_cast<T>(lua_tonumber(L, -1));

            lua_pop(L, 1);
            return opt;
        }

        lua_State* L;
    };

    struct LuaConfig
    {
        LuaConfig(lua_State* L, const char* configContent)
            : L(L)
        {
            int code = luaL_dostring(L, configContent);
            //if (code != 0)
            //    logError("Error: {}\n", lua_tostring(L, -1));
        }

        template <typename T>
        std::optional<T> get(const char* variableName)
        {
            lua_getglobal(L, variableName);

            std::optional<T> opt;
            if (lua_isnumber(L, -1))
                opt = static_cast<T>(lua_tonumber(L, -1));

            lua_pop(L, 1);
            return opt;
        }

        std::unique_ptr<TableProxy> getTable(const char* variableName)
        {
            lua_getglobal(L, variableName);
            if (!lua_istable(L, -1))
            {
                lua_pop(L, 1);
                return nullptr;
            }

            return std::make_unique<TableProxy>(L);
        }

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

        lua_State* L;
    };

    struct MyPoint
    {
        int x;
        int y;

        MyPoint() {}
        MyPoint(int x, int y) : x(x), y(y) {}
    };

    int newPoint(lua_State* L)
    {
        int x = luaL_checkint(L, 1);
        int y = luaL_checkint(L, 2);

        std::size_t numBytes = sizeof(MyPoint);
        void* mem = lua_newuserdata(L, numBytes);
        MyPoint* point = new(mem) MyPoint();
        point->x = x;
        point->y = y;

        luaL_getmetatable(L, "MyPoint.MF");
        lua_setmetatable(L, -2);

        return 1;
    }


    template <typename T, typename... Args>
    T* createLuaObject(lua_State* L, Args&&... args)
    {
        T** ptr = static_cast<T**>(lua_newuserdata(L, sizeof(T*)));
        *ptr = new MyPoint(std::forward<Args>(args)...);

        return *ptr;
    }

    int createPoint(lua_State* L)
    {
        int x = luaL_checkint(L, 1);
        int y = luaL_checkint(L, 2);

        /*MyPoint* point = */createLuaObject<MyPoint>(L, x, y);

        luaL_getmetatable(L, "Tut.MyPoint");
        lua_setmetatable(L, -2);

        return 1;
    }

    MyPoint* checkMyPoint(lua_State* L, int index)
    {
        return *static_cast<MyPoint**>(luaL_checkudata(L, index, "Tut.MyPoint"));
    }

    int destroyPoint(lua_State* L)
    {
        MyPoint* point = checkMyPoint(L, 1);
        delete point;
        return 0;
    }

    int setPointX(lua_State* L)
    {
        luaL_checkudata(L, 1, "MyPoint.MF");
        MyPoint* point = static_cast<MyPoint*>(lua_touserdata(L, 1));
        int x = luaL_checkint(L, 2);
        point->x = x;
        return 0;
    }

    int setPointX2(lua_State* L)
    {
        MyPoint* point = checkMyPoint(L, 1);
        point->x = luaL_checkint(L, 2);
        return 0;
    }

    int getPointX(lua_State* L)
    {
        luaL_checkudata(L, 1, "MyPoint.MF");
        MyPoint* point = static_cast<MyPoint*>(lua_touserdata(L, 1));
        lua_pushinteger(L, point->x);
        return 1;
    }

    int getProd(lua_State* L)
    {
        luaL_checkudata(L, 1, "MyPoint.MF");
        MyPoint* point = static_cast<MyPoint*>(lua_touserdata(L, 1));
        lua_pushinteger(L, point->x * point->y);
        return 1;
    }

    int getProd2(lua_State* L)
    {
        MyPoint* point = checkMyPoint(L, 1);
        lua_pushinteger(L, point->x * point->y);
        return 1;
    }

    int pointToString(lua_State* L)
    {
        luaL_checkudata(L, 1, "MyPoint.MF");
        MyPoint* point = static_cast<MyPoint*>(lua_touserdata(L, 1));
        lua_pushfstring(L, "[ %d, %d ]", point->x, point->y);
        return 1;
    }

    const luaL_Reg pointFunctions [] =
    {
        { "new", newPoint },
        { nullptr, nullptr }
    };

    const luaL_Reg pointMemFunctions[] =
    {
        { "setX", setPointX },
        { "getX", getPointX },
        { "prod", getProd },
        { "__tostring", pointToString },
        { nullptr, nullptr }
    };

    std::string pointScript = "p = MyPoint.new(10, 20)\n"
        "print(p:getX())\n"
        "p:setX(20)\n"
        "print(p:prod())\n"
        "print(p)\n"
        "res = p:prod()\n"
        ;

    std::string pointScript2 = "p = MyPoint.new(10, 20)\n"
        "print(p:prod())\n"
        "p:setX(20)\n"
        "print(p)\n"
        "res = p:prod()\n"
        ;

    const luaL_Reg funs[] =
    {
        { "__call", createPoint },
        { "new", createPoint },
        { "setX", setPointX2 },
        { "prod", getProd2 },
        { "__gc", destroyPoint }
    };

    LuaMachine::LuaMachine()
        : m_state(lua_open())
    {
        luaL_openlibs(m_state);

        /*luaL_newmetatable(m_state, "Tut.MyPoint");
        luaL_register(m_state, nullptr, funs);

        lua_pushvalue(m_state, -1);

        lua_setfield(m_state, -2, "__index");
        lua_setglobal(m_state, "MyPoint");

        LuaConfig config(m_state, pointScript2.c_str());
        int res = config.get<int>("res").value_or(-1);
        logDebug("Result: ", res);*/

        //luaL_openlibs(m_state);
        //
        //luaL_newmetatable(m_state, "MyPoint.MF");
        //lua_pushvalue(m_state, -1); // duplicate metatable ptr to the top of the stack
        //lua_setfield(m_state, -2, "__index"); // set metatable as its own __index field
        //luaL_register(m_state, nullptr, pointMemFunctions);
        //
        //luaL_register(m_state, "MyPoint", pointFunctions);
        //
        //LuaConfig config(m_state, pointScript.c_str());
        //
        //int res = config.get<int>("res").value_or(-1);
        //logDebug("Result: ", res);

        //registerType<Point>(m_state);

        //LuaConfig config(m_state, tinyScript.c_str());
        //
        //auto width  = config.get<int>("width");
        //auto height = config.get<int>("height");
        //
        //auto table = config.getTable("background");
        //float r = table->get<float>("r").value_or(0.0f);
        //float g = table->get<float>("g").value_or(0.0f);
        //float b = table->get<float>("b").value_or(0.0f);


        //logInfo("Made it!");
    }

    LuaMachine::~LuaMachine()
    {
        lua_gc(m_state, LUA_GCCOLLECT, 0);
        lua_close(m_state);
    }

    void LuaMachine::runScript(const std::filesystem::path& /*scriptPath*/)
    {
    }
}