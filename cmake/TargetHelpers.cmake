# Enables C++23 and high amount of warnings for a C++ target.
function(enable_default_cpp_compile_options targetName optionType)
    set_target_properties(${targetName} PROPERTIES LANGUAGE CXX LINKER_LANGUAGE CXX)
    # Propagate C++ standard requirement to consumers, but keep compiler flags private.
    # INTERFACE targets (header-only libs) skip flags entirely since they have no sources.
    target_compile_features(${targetName} ${optionType} cxx_std_23)

    if(NOT optionType STREQUAL "INTERFACE")
        if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC") # no Clang no Intel
            target_compile_options(${targetName} PRIVATE /MP /JMC /Zi /W4 /Zc:preprocessor)
        else()
            target_compile_options(${targetName} PRIVATE -W -Wall)
        endif()
    endif()
endfunction()

function(add_crisp_alias_target targetName)
    string(SUBSTRING "${targetName}" 0 5 targetPrefix)

    if(targetPrefix STREQUAL "Crisp")
        string(SUBSTRING "${targetName}" 5 -1 subTargetName)
        add_library("Crisp::${subTargetName}" ALIAS ${targetName})
    endif()
endfunction()

# Creates a C++ static library target.
function(add_cpp_static_library targetName)
    add_library(${targetName} STATIC ${ARGN})
    add_crisp_alias_target(${targetName})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
    target_include_directories(${targetName} PUBLIC ${CRISP_INCLUDE_DIR})
endfunction()

# Creates a C++ shared library target.
function(add_cpp_shared_library targetName)
    add_library(${targetName} SHARED ${ARGN})
    add_crisp_alias_target(${targetName})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
    target_include_directories(${targetName} PUBLIC ${CRISP_INCLUDE_DIR})
endfunction()

# Creates a C++ header-only library target.
function(add_cpp_header_library targetName)
    add_library(${targetName} INTERFACE ${ARGN})
    add_crisp_alias_target(${targetName})
    enable_default_cpp_compile_options(${targetName} INTERFACE)
    target_include_directories(${targetName} INTERFACE ${CRISP_INCLUDE_DIR})
endfunction()

# Creates a C++ binary target.
function(add_cpp_binary targetName)
    add_executable(${targetName} ${ARGN})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
endfunction()

# Creates a C++ binary test target. No-op when CRISP_BUILD_TESTS is OFF.
function(add_cpp_test targetName)
    if(NOT CRISP_BUILD_TESTS)
        return()
    endif()
    add_executable(${targetName} ${ARGN})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
    target_link_libraries(${targetName} PRIVATE GTest::gmock)
    target_link_libraries(${targetName} PRIVATE GTest::gmock_main)

    add_test(NAME ${targetName} COMMAND $<TARGET_FILE:${targetName}>)
endfunction()

# Creates a C++ binary benchmark target. No-op when CRISP_BUILD_BENCHMARKS is OFF.
function(add_cpp_benchmark targetName)
    if(NOT CRISP_BUILD_BENCHMARKS)
        return()
    endif()
    add_executable(${targetName} ${ARGN})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
    target_link_libraries(${targetName} PRIVATE benchmark::benchmark)
endfunction()

# Copies a list of shared libraries into the designated target's directory. To be used with DLL dependencies.
function(copy_shared_libs targetName)
    foreach(sharedLib ${ARGN})
        add_custom_command(TARGET ${targetName} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${sharedLib}> $<TARGET_FILE_DIR:${targetName}>)
    endforeach()
endfunction()
