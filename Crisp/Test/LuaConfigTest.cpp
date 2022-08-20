#include "gtest/gtest.h"

#include "Crisp/LuaConfig.hpp"

namespace
{

}

TEST(LuaConfigTest, Basic)
{
    const char* script = "x = 3";

    crisp::LuaConfig lua;
    lua.openScript(script);

    ASSERT_EQ(lua.get<int>("x").value(), 3);
}

TEST(LuaConfigTest, Tables)
{
    const char* script = "x = 3\n\
        spells = {\
        blessing=10,\
        fireball=15.3\
        }\n\
        text=\"awesome\"";

    crisp::LuaConfig lua;
    lua.openScript(script);

    ASSERT_EQ(lua.get<int>("x").value(), 3);

    auto tableRef = lua["spells"];

    ASSERT_EQ(tableRef.get<int>("blessing").value(), 10);
    ASSERT_EQ(tableRef.get<double>("fireball").value(), 15.3);
    ASSERT_EQ(lua.get<std::string>("text").value(), "awesome");

    ASSERT_EQ(lua.hasVariable("spells"), true);
    ASSERT_EQ(lua.hasVariable("text"), true);
    ASSERT_EQ(lua.hasVariable("y"), false);
}
