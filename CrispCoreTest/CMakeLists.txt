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
target_link_libraries(CrispCoreTest PRIVATE Crisp)

target_compile_features(CrispCoreTest PUBLIC cxx_std_20)