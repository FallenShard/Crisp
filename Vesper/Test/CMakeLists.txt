add_cpp_test(VesperTest
    "Distribution1DTest.cpp"
    "EmbreeTest.cpp"
    "TbbTest.cpp"
)
target_link_libraries(VesperTest PRIVATE Vesper)
