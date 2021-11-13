add_executable(CrispTest
    "CameraTests/CameraTest.cpp"
    "CameraTests/FreeCameraControllerTest.cpp"
    "ShadingLanguageTests/LexerTest.cpp"
    "VulkanTests/VulkanContextTest.cpp")

set_target_properties(CrispTest PROPERTIES
    CXX_STANDARD 20)

target_link_libraries(CrispTest PRIVATE gtest)
target_link_libraries(CrispTest PRIVATE gtestMain)
target_link_libraries(CrispTest PRIVATE Crisp)

function(copy_shared_libs targetNames)
    foreach(targetName ${ARGV})
        add_custom_command(TARGET CrispTest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${targetName}> $<TARGET_FILE_DIR:CrispTest>
        )
    endforeach()
endfunction()

copy_shared_libs(vulkan lua freetype zlib tbb tbbMalloc embree)
