file(GLOB_RECURSE CrispSources "*.cpp")
file(GLOB_RECURSE CrispHeaders "*.hpp")

add_executable(Crisp ${CrispSources} ${CrispHeaders})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
             FILES ${CrispSources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
             FILES ${CrispHeaders})

target_link_libraries(Crisp PUBLIC vulkan)
target_link_libraries(Crisp PUBLIC glfw)
target_link_libraries(Crisp PUBLIC stb)
target_link_libraries(Crisp PUBLIC lua)
target_link_libraries(Crisp PUBLIC freetype)
target_link_libraries(Crisp PUBLIC gzip)
target_link_libraries(Crisp PUBLIC zlib)

target_link_libraries(Crisp PUBLIC CrispCore)
target_link_libraries(Crisp PUBLIC Vesper)

target_include_directories(Crisp PUBLIC ".")

target_compile_features(Crisp PUBLIC cxx_std_17)

add_custom_command(TARGET Crisp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:vulkan> $<TARGET_FILE_DIR:Crisp>
)

add_custom_command(TARGET Crisp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:lua> $<TARGET_FILE_DIR:Crisp>
)

add_custom_command(TARGET Crisp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:freetype> $<TARGET_FILE_DIR:Crisp>
)

add_custom_command(TARGET Crisp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:zlib> $<TARGET_FILE_DIR:Crisp>
)

add_custom_command(TARGET Crisp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:tbb> $<TARGET_FILE_DIR:Crisp>
)

add_custom_command(TARGET Crisp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:tbbMalloc> $<TARGET_FILE_DIR:Crisp>
)

add_custom_command(TARGET Crisp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:embree> $<TARGET_FILE_DIR:Crisp>
)
