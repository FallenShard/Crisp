cmake_minimum_required(VERSION 3.16)

project(CrispProject VERSION 0.1.0 LANGUAGES CXX)

if(MSVC)
  add_definitions(/MP)
endif()

# This is a single line comment

add_subdirectory(Externals/lua)
add_subdirectory(Externals/stb)
add_subdirectory(Externals/glfw)
add_subdirectory(Externals/vulkan)
add_subdirectory(Externals/glm)
add_subdirectory(Externals/fmt)
add_subdirectory(Externals/tinyexr)
add_subdirectory(Externals/rlutil)
add_subdirectory(Externals/embree3)
add_subdirectory(Externals/tbb)
add_subdirectory(Externals/rapidxml)
add_subdirectory(Externals/freetype)
add_subdirectory(Externals/gzip)
add_subdirectory(Externals/zlib)
add_subdirectory(Externals/gtest)

add_subdirectory(CrispCore)
add_subdirectory(CrispCoreTest)

add_subdirectory(Vesper)
add_subdirectory(VesperTest)

add_subdirectory(Crisp)
