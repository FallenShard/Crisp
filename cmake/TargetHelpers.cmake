# Enables C++23 and high amount of warnings for a C++ target.
function(enable_default_cpp_compile_options targetName optionType)
    # Propagate C++ standard requirement to consumers, but keep compiler flags private.
    # INTERFACE targets (header-only libs) skip flags entirely since they have no sources.
    target_compile_features(${targetName} ${optionType} cxx_std_23)

    if(NOT optionType STREQUAL "INTERFACE")
        if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC") # no Clang no Intel
            # /JMC             - Just My Code in Debug builds.
            # /Zi              - emit Debug information into a PDB.
            # /W4              - high warning level.
            # /Zc:preprocessor - conforming preprocessor instead of MSVC's legacy one.
            # /Zc:__cplusplus  - report the real __cplusplus; MSVC otherwise hardcodes 199711L.
            # /EHsc            - standard C++ exceptions; assume extern "C" never throws.
            # /utf-8           - read sources and encode narrow literals as UTF-8.
            #
            # The last two are also exported PUBLIC by spdlog/fmt, so setting them here is what
            # keeps every target consistent rather than only those that happen to link spdlog.
            # Headers branching on __cplusplus (e.g. GSL's gsl::byte) otherwise differ per TU.
            target_compile_options(${targetName}
                PRIVATE
                    /W4 /Zc:preprocessor /Zc:__cplusplus /EHsc /utf-8
                    $<$<CONFIG:Debug>:/JMC>
                    $<$<CONFIG:Debug>:/Zi>)
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
    set_target_properties(${targetName} PROPERTIES FOLDER "Crisp/Libraries")
endfunction()

# Creates a C++ shared library target.
function(add_cpp_shared_library targetName)
    add_library(${targetName} SHARED ${ARGN})
    add_crisp_alias_target(${targetName})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
    target_include_directories(${targetName} PUBLIC ${CRISP_INCLUDE_DIR})
    set_target_properties(${targetName} PROPERTIES FOLDER "Crisp/Libraries")
endfunction()

# Creates a C++ header-only library target.
function(add_cpp_header_library targetName)
    add_library(${targetName} INTERFACE ${ARGN})
    add_crisp_alias_target(${targetName})
    enable_default_cpp_compile_options(${targetName} INTERFACE)
    target_include_directories(${targetName} INTERFACE ${CRISP_INCLUDE_DIR})
    set_target_properties(${targetName} PROPERTIES FOLDER "Crisp/Libraries")
endfunction()

# Creates a C++ binary target.
function(add_cpp_binary targetName)
    add_executable(${targetName} ${ARGN})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
    set_target_properties(${targetName} PROPERTIES FOLDER "Crisp/Applications")
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
    set_target_properties(${targetName} PROPERTIES FOLDER "Crisp/Tests")

    gtest_discover_tests(
        ${targetName}
        TEST_PREFIX ${targetName}.
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endfunction()

# Copies a tracked fixture into a target-specific directory in the build tree.
function(stage_test_file targetName sourceFile relativeOutputPath)
    if(NOT TARGET ${targetName})
        return()
    endif()

    get_filename_component(sourcePath "${sourceFile}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(outputPath "${CMAKE_CURRENT_BINARY_DIR}/TestData/${targetName}/${relativeOutputPath}")
    get_filename_component(outputDirectory "${outputPath}" DIRECTORY)
    file(MAKE_DIRECTORY "${outputDirectory}")
    configure_file("${sourcePath}" "${outputPath}" COPYONLY)
endfunction()

# Compiles a tracked GLSL fixture to SPIR-V in a target-specific build directory.
function(add_test_shader targetName sourceFile)
    if(NOT TARGET ${targetName})
        return()
    endif()

    if(NOT TARGET glslang-standalone)
        message(FATAL_ERROR "add_test_shader requires the glslang-standalone target")
    endif()

    get_filename_component(sourcePath "${sourceFile}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(sourceName "${sourceFile}" NAME)
    set(outputDirectory "${CMAKE_CURRENT_BINARY_DIR}/TestData/${targetName}")
    set(outputPath "${outputDirectory}/${sourceName}.spv")

    add_custom_command(
        OUTPUT "${outputPath}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${outputDirectory}"
        COMMAND $<TARGET_FILE:glslang-standalone>
            --target-env vulkan1.3 -V "${sourcePath}" -o "${outputPath}"
        DEPENDS "${sourcePath}" glslang-standalone
        VERBATIM)
    set_source_files_properties("${outputPath}" PROPERTIES GENERATED TRUE)
    target_sources(${targetName} PRIVATE "${outputPath}")
endfunction()

# Creates a C++ binary benchmark target. No-op when CRISP_BUILD_BENCHMARKS is OFF.
function(add_cpp_benchmark targetName)
    if(NOT CRISP_BUILD_BENCHMARKS)
        return()
    endif()

    add_executable(${targetName} ${ARGN})
    enable_default_cpp_compile_options(${targetName} PUBLIC)
    target_link_libraries(${targetName} PRIVATE benchmark::benchmark)
    set_target_properties(${targetName} PROPERTIES FOLDER "Crisp/Benchmarks")
endfunction()

# Copies a list of shared libraries into the designated target's directory. To be used with DLL dependencies.
function(copy_shared_libs targetName)
    foreach(sharedLib ${ARGN})
        add_custom_command(TARGET ${targetName} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${sharedLib}> $<TARGET_FILE_DIR:${targetName}>)
    endforeach()
endfunction()
