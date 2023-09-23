# Default include path for all Crisp targets.
set(CRISP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(Crisp/Animation)
add_subdirectory(Crisp/Camera)
add_subdirectory(Crisp/Core)
add_subdirectory(Crisp/Geometry)
add_subdirectory(Crisp/GUI)
add_subdirectory(Crisp/Image)
add_subdirectory(Crisp/IO)
add_subdirectory(Crisp/Lights)
add_subdirectory(Crisp/Materials)
add_subdirectory(Crisp/Math)
add_subdirectory(Crisp/Mesh)
add_subdirectory(Crisp/Models)
add_subdirectory(Crisp/Optics)
add_subdirectory(Crisp/PathTracer)
add_subdirectory(Crisp/Renderer)
add_subdirectory(Crisp/Scenes)
add_subdirectory(Crisp/ShadingLanguage)
add_subdirectory(Crisp/Spectra)
add_subdirectory(Crisp/Vulkan)
add_subdirectory(Crisp/Utils)

add_cpp_binary(CrispMain main.cpp)
set_target_properties(CrispMain PROPERTIES
    LINK_FLAGS "/INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF"
    COMPILE_PDB_NAME CrispMain
    COMPILE_PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
)
target_link_libraries(CrispMain PRIVATE Crisp::Application)
copy_shared_libs(CrispMain freetype embree TBB::tbb)
target_sources(CrispMain PRIVATE Crisp.natvis)
