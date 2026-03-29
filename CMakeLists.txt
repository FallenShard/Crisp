cmake_minimum_required(VERSION 3.28)
project(Crisp VERSION 0.1.0 LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

enable_testing()

include(cmake/TargetHelpers.cmake)
include(cmake/Dependencies.cmake)

add_subdirectory(Crisp)

# Set the start-up project to be the main executable CrispMain
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CrispMain)
