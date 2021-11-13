add_executable(CrispMain main.cpp)

set_target_properties(CrispMain PROPERTIES
    CXX_STANDARD 20
    LINK_FLAGS "/INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF"
    COMPILE_PDB_NAME CrispMain
    COMPILE_PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
    VS_DEBUGGER_COMMAND_ARGUMENTS "config=D:/version-control/Crisp/Config.lua scene=3"
)

target_link_libraries(CrispMain PRIVATE Crisp)

target_include_directories(CrispMain PUBLIC ".")

function(copy_shared_libs targetNames)
    foreach(targetName ${ARGV})
        add_custom_command(TARGET CrispMain POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${targetName}> $<TARGET_FILE_DIR:CrispMain>
        )
    endforeach()
endfunction()

copy_shared_libs(vulkan lua freetype zlib tbb tbbMalloc embree)
