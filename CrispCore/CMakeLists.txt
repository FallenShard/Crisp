file(GLOB_RECURSE CrispCoreSources "*.cpp")
file(GLOB_RECURSE CrispCoreHeaders "*.hpp")

add_library(CrispCore ${CrispCoreSources} ${CrispCoreHeaders})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
             FILES ${CrispCoreSources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
             FILES ${CrispCoreHeaders})

target_link_libraries(CrispCore PUBLIC glm)
target_link_libraries(CrispCore PUBLIC fmt)
target_link_libraries(CrispCore PUBLIC spdlog)
target_link_libraries(CrispCore PUBLIC tinyexr)

target_include_directories(CrispCore PUBLIC ".")

target_compile_features(CrispCore PUBLIC cxx_std_20)