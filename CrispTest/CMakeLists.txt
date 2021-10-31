file(GLOB_RECURSE CrispTestSources "*.cpp")
file(GLOB_RECURSE CrispTestHeaders "*.hpp")

add_executable(CrispTest ${CrispTestSources} ${CrispTestHeaders})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CrispTestSources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CrispTestHeaders})

target_link_libraries(CrispTest PRIVATE gtest)
target_link_libraries(CrispTest PRIVATE gtestMain)
target_link_libraries(CrispTest PRIVATE Crisp)

set_target_properties(CrispTest PROPERTIES
    CXX_STANDARD 23)

add_custom_command(TARGET CrispTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:vulkan> $<TARGET_FILE_DIR:CrispTest>
)

add_custom_command(TARGET CrispTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:lua> $<TARGET_FILE_DIR:CrispTest>
)

add_custom_command(TARGET CrispTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:freetype> $<TARGET_FILE_DIR:CrispTest>
)

add_custom_command(TARGET CrispTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:zlib> $<TARGET_FILE_DIR:CrispTest>
)

add_custom_command(TARGET CrispTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:tbb> $<TARGET_FILE_DIR:CrispTest>
)

add_custom_command(TARGET CrispTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:tbbMalloc> $<TARGET_FILE_DIR:CrispTest>
)

add_custom_command(TARGET CrispTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:embree> $<TARGET_FILE_DIR:CrispTest>
)
