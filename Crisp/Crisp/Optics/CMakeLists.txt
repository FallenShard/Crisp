add_cpp_interface_library(CrispOptics
    "Fresnel.hpp")
target_link_libraries(CrispOptics
    INTERFACE CrispSpectrum)