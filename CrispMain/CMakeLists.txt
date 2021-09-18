file(GLOB_RECURSE CrispMainSources "*.cpp")
file(GLOB_RECURSE CrispMainHeaders "*.hpp")

add_executable(CrispMain ${CrispMainSources} ${CrispMainHeaders})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CrispMainSources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CrispMainHeaders})

target_compile_options(CrispMain PRIVATE /Zi)

# Tell linker to include symbol data
set_target_properties(CrispMain PROPERTIES
    LINK_FLAGS "/INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF"
)

# Set file name & location
set_target_properties(CrispMain PROPERTIES
    COMPILE_PDB_NAME CrispMain
    COMPILE_PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
    VS_DEBUGGER_COMMAND_ARGUMENTS "config=../../Config.lua scene=3"
)

target_link_libraries(CrispMain PUBLIC Crisp)

target_include_directories(CrispMain PUBLIC ".")

target_compile_features(CrispMain PUBLIC cxx_std_20)

add_custom_command(TARGET CrispMain POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:vulkan> $<TARGET_FILE_DIR:CrispMain>
)

add_custom_command(TARGET CrispMain POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:lua> $<TARGET_FILE_DIR:CrispMain>
)

add_custom_command(TARGET CrispMain POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:freetype> $<TARGET_FILE_DIR:CrispMain>
)

add_custom_command(TARGET CrispMain POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:zlib> $<TARGET_FILE_DIR:CrispMain>
)

add_custom_command(TARGET CrispMain POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:tbb> $<TARGET_FILE_DIR:CrispMain>
)

add_custom_command(TARGET CrispMain POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:tbbMalloc> $<TARGET_FILE_DIR:CrispMain>
)

add_custom_command(TARGET CrispMain POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:embree> $<TARGET_FILE_DIR:CrispMain>
)
