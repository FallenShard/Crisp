file(GLOB_RECURSE CrispCoreTestSources "*.cpp")
file(GLOB_RECURSE CrispCoreTestHeaders "*.hpp")

add_executable(CrispCoreTest ${CrispCoreTestSources} ${CrispCoreTestHeaders})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
             FILES ${CrispCoreTestSources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
             FILES ${CrispCoreTestHeaders})

target_link_libraries(CrispCoreTest PRIVATE gtest)
target_link_libraries(CrispCoreTest PRIVATE gtestMain)
target_link_libraries(CrispCoreTest PRIVATE CrispCore)

set_target_properties(CrispCoreTest PROPERTIES
    CXX_STANDARD 23)

add_custom_command(TARGET CrispCoreTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:lua> $<TARGET_FILE_DIR:CrispCoreTest>
)
