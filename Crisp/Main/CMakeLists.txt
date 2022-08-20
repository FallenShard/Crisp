add_cpp_binary(CrispMain main.cpp)
set_target_properties(CrispMain PROPERTIES
    LINK_FLAGS "/INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF"
    COMPILE_PDB_NAME CrispMain
    COMPILE_PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
    VS_DEBUGGER_COMMAND_ARGUMENTS "config=D:/version-control/Crisp/Config.lua scene=3"
)
target_link_libraries(CrispMain PRIVATE CrispApplication)
copy_shared_libs(CrispMain vulkan lua freetype zlib tbb tbbMalloc embree)
