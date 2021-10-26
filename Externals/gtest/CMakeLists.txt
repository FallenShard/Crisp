add_library(gtest STATIC IMPORTED GLOBAL)
set_target_properties(gtest PROPERTIES IMPORTED_LOCATION_DEBUG
    ${PROJECT_SOURCE_DIR}/Externals/gtest/gtestd.lib)
set_target_properties(gtest PROPERTIES IMPORTED_LOCATION_RELEASE
    ${PROJECT_SOURCE_DIR}/Externals/gtest/gtest.lib)
set_target_properties(gtest PROPERTIES IMPORTED_LOCATION_RELWITHDEBINFO
    ${PROJECT_SOURCE_DIR}/Externals/gtest/gtest.lib)
set_target_properties(gtest PROPERTIES IMPORTED_LOCATION_MINSIZEREL
    ${PROJECT_SOURCE_DIR}/Externals/gtest/gtest.lib)
target_include_directories(gtest INTERFACE "include/")

add_library(gtestMain STATIC IMPORTED GLOBAL)
set_target_properties(gtestMain PROPERTIES IMPORTED_LOCATION_DEBUG
    ${PROJECT_SOURCE_DIR}/Externals/gtest/gtest_maind.lib)
set_target_properties(gtestMain PROPERTIES IMPORTED_LOCATION_RELEASE
    ${PROJECT_SOURCE_DIR}/Externals/gtest/gtest_main.lib)
set_target_properties(gtestMain PROPERTIES IMPORTED_LOCATION_RELWITHDEBINFO
    ${PROJECT_SOURCE_DIR}/Externals/gtest/gtest_main.lib)
set_target_properties(gtestMain PROPERTIES IMPORTED_LOCATION_MINSIZEREL
    ${PROJECT_SOURCE_DIR}/Externals/gtest/gtest_main.lib)
target_include_directories(gtestMain INTERFACE "include/")