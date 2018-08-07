# Crisp
This project contains a small application that will showcase reference implementations of various algorithms for ray tracing and
real-time rendering that I have encountered in computer graphics, both during my studies and in my career.

It is developed sporadically during my free time so updates are not going to be very frequent.

Currently, the following techniques have an implementation for the real-time renderer:
  - SSAO
  - SPH simulation with compute shaders
  - Shadow maps (standard, PCF, variance, cascaded)
  - Simple physically-based shading model
  - Screen-space reflections
  - Normal mapping
  - Simple point sprite rendering
  
The ray tracing section of the code (codenamed Vesper in the source) has the following features:
  - BRDFs
    - Lambertian
    - Oren Nayar
    - Rough & smooth conductor
    - Rough & smooth dielectric
  - Integrators
    - Ambient occlusion
    - Direct lighting
    - Path tracing with MIS
  - Point, area, environment lighting
  
  Some pictures and more information are listed at the [project page](https://fallenshard.github.io/crisp-home.html)
