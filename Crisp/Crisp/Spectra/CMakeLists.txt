add_cpp_static_library(CrispSpectrum
    "RgbSpectrum.hpp"
    "RgbSpectrum.cpp"
    "Spectrum.hpp"
    "WeightedRgbSpectrum.hpp"
)
target_link_libraries(CrispSpectrum
    PUBLIC CrispMath)