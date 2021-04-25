file(GLOB_RECURSE VesperSources "*.cpp")
file(GLOB_RECURSE VesperHeaders "*.hpp")

add_library(Vesper ${VesperSources} ${VesperHeaders})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${VesperSources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${VesperHeaders})

# Third-party libraries
target_link_libraries(Vesper PUBLIC tbb)
target_link_libraries(Vesper PUBLIC embree)
target_link_libraries(Vesper PUBLIC tinyexr)
target_link_libraries(Vesper PUBLIC rapidxml)

# Solution libraries
target_link_libraries(Vesper PUBLIC CrispCore)

target_include_directories(Vesper PRIVATE "./Vesper")
target_include_directories(Vesper PUBLIC ".")

target_compile_features(Vesper PUBLIC cxx_std_20)
target_compile_definitions(Vesper PUBLIC __TBB_NO_IMPLICIT_LINKAGE)