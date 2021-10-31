file(GLOB_RECURSE CrispSources "*.cpp")
file(GLOB_RECURSE CrispHeaders "*.hpp")

add_library(Crisp ${CrispSources} ${CrispHeaders})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CrispSources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CrispHeaders})

# Set file name & location
set_target_properties(Crisp PROPERTIES
    LINK_FLAGS "/INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF"
    COMPILE_PDB_NAME Crisp
    COMPILE_PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
    CXX_STANDARD 23
)

target_link_libraries(Crisp PUBLIC vulkan)
target_link_libraries(Crisp PUBLIC glfw)
target_link_libraries(Crisp PUBLIC stb)
target_link_libraries(Crisp PUBLIC lua)
target_link_libraries(Crisp PUBLIC freetype)
target_link_libraries(Crisp PUBLIC gzip)
target_link_libraries(Crisp PUBLIC zlib)

target_link_libraries(Crisp PUBLIC CrispCore)
target_link_libraries(Crisp PUBLIC Vesper)

target_include_directories(Crisp PUBLIC ".")
