add_library(CrispCore STATIC
    "CrispCore/IO/MeshReader.cpp"
    "CrispCore/IO/OpenEXRReader.cpp"
    "CrispCore/IO/OpenEXRWriter.cpp"
    "CrispCore/IO/WavefrontObjReader.cpp"
    "CrispCore/Math/Distribution1D.cpp"
    "CrispCore/Math/Distribution2D.cpp"
    "CrispCore/Math/Warp.cpp"
    "CrispCore/Mesh/TriangleMesh.cpp"
    "CrispCore/Mesh/VertexAttributeDescriptor.cpp"
    "CrispCore/CommandLineParser.cpp"
    "CrispCore/StringUtils.cpp"

    "CrispCore/IO/MeshReader.hpp"
    "CrispCore/IO/OpenEXRReader.hpp"
    "CrispCore/IO/OpenEXRWriter.hpp"
    "CrispCore/IO/WavefrontObjReader.hpp"
    "CrispCore/Math/BoundingBox.hpp"
    "CrispCore/Math/Constants.hpp"
    "CrispCore/Math/CoordinateFrame.hpp"
    "CrispCore/Math/Distribution1D.hpp"
    "CrispCore/Math/Distribution2D.hpp"
    "CrispCore/Math/Headers.hpp"
    "CrispCore/Math/Octree.hpp"
    "CrispCore/Math/Operations.hpp"
    "CrispCore/Math/Ray.hpp"
    "CrispCore/Math/Rect.hpp"
    "CrispCore/Math/Transform.hpp"
    "CrispCore/Math/Warp.hpp"
    "CrispCore/Mesh/TriangleMesh.hpp"
    "CrispCore/Mesh/TriangleMeshView.hpp"
    "CrispCore/Mesh/VertexAttributeBuffer.hpp"
    "CrispCore/Mesh/VertexAttributeDescriptor.hpp"
    "CrispCore/Mesh/VertexAttributeTraits.hpp"
    "CrispCore/BitFlags.hpp"
    "CrispCore/ChromeProfiler.hpp"
    "CrispCore/CommandLineParser.hpp"
    "CrispCore/ConnectionHandler.hpp"
    "CrispCore/Delegate.hpp"
    "CrispCore/Event.hpp"
    "CrispCore/GlmFormatters.hpp"
    "CrispCore/Logger.hpp"
    "CrispCore/LuaConfig.hpp"
    "CrispCore/Profiler.hpp"
    "CrispCore/RobinHood.hpp"
    "CrispCore/StringUtils.hpp"
    "CrispCore/Timer.hpp"
)

target_include_directories(CrispCore PUBLIC ".")

target_link_libraries(CrispCore PRIVATE tinyexr)
target_link_libraries(CrispCore PUBLIC glm)
target_link_libraries(CrispCore PUBLIC fmt)
target_link_libraries(CrispCore PUBLIC spdlog)
target_link_libraries(CrispCore PUBLIC robin_hood)
target_link_libraries(CrispCore PUBLIC lua)

set_target_properties(CrispCore PROPERTIES
    CXX_STANDARD 23)