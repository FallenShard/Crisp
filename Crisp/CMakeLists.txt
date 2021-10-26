file(GLOB_RECURSE CrispSources "*.cpp")
file(GLOB_RECURSE CrispHeaders "*.hpp")

add_library(Crisp ${CrispSources} ${CrispHeaders})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CrispSources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CrispHeaders})

target_compile_options(Crisp PRIVATE /Zi)

# Tell linker to include symbol data
set_target_properties(Crisp PROPERTIES
    LINK_FLAGS "/INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF"
)

# Set file name & location
set_target_properties(Crisp PROPERTIES
    COMPILE_PDB_NAME Crisp
    COMPILE_PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
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

target_compile_features(Crisp PUBLIC cxx_std_23)

# add_custom_command(TARGET Crisp POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#     $<TARGET_FILE:vulkan> $<TARGET_FILE_DIR:Crisp>
# )

# add_custom_command(TARGET Crisp POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#     $<TARGET_FILE:lua> $<TARGET_FILE_DIR:Crisp>
# )

# add_custom_command(TARGET Crisp POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#     $<TARGET_FILE:freetype> $<TARGET_FILE_DIR:Crisp>
# )

# add_custom_command(TARGET Crisp POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#     $<TARGET_FILE:zlib> $<TARGET_FILE_DIR:Crisp>
# )

# add_custom_command(TARGET Crisp POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#     $<TARGET_FILE:tbb> $<TARGET_FILE_DIR:Crisp>
# )

# add_custom_command(TARGET Crisp POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#     $<TARGET_FILE:tbbMalloc> $<TARGET_FILE_DIR:Crisp>
# )

# add_custom_command(TARGET Crisp POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#     $<TARGET_FILE:embree> $<TARGET_FILE_DIR:Crisp>
# )
