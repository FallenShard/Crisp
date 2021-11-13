add_executable(CrispCoreTest
    "BitFlagsTest.cpp"
    "CommandLineParserTest.cpp"
    "DelegateTest.cpp"
    "EventTest.cpp"
    "GlmStructuredBindingsTest.cpp"
    "LuaConfigTest.cpp"
    "StringUtilsTest.cpp"
    "WavefrontObjTest.cpp")

target_link_libraries(CrispCoreTest PRIVATE gtest)
target_link_libraries(CrispCoreTest PRIVATE gtestMain)
target_link_libraries(CrispCoreTest PRIVATE CrispCore)

set_target_properties(CrispCoreTest PROPERTIES
    CXX_STANDARD 20)

add_custom_command(TARGET CrispCoreTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:lua> $<TARGET_FILE_DIR:CrispCoreTest>
)
