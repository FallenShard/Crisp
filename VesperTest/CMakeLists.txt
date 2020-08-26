file(GLOB_RECURSE VesperTestSources "*.cpp")
file(GLOB_RECURSE VesperTestHeaders "*.hpp")

add_executable(VesperTest ${VesperTestSources} ${VesperTestHeaders})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
             FILES ${VesperTestSources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
             FILES ${VesperTestHeaders})

target_link_libraries(VesperTest PRIVATE gtest)
target_link_libraries(VesperTest PRIVATE gtestMain)
target_link_libraries(VesperTest PRIVATE Vesper)

target_compile_features(VesperTest PUBLIC cxx_std_17)

add_custom_command(TARGET VesperTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE_DIR:tbb>\\tbb.dll $<TARGET_FILE_DIR:VesperTest>
)

add_custom_command(TARGET VesperTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:tbbMalloc> $<TARGET_FILE_DIR:VesperTest>
)

add_custom_command(TARGET VesperTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:embree> $<TARGET_FILE_DIR:VesperTest>
)
