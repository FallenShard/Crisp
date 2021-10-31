# Crisp

This project contains a small application that showcases reference implementations of various algorithms for ray tracing and real-time rendering that I have encountered in computer graphics, both during my studies and in my career.

It is developed sporadically during my free time so updates are not very frequent.
The real-time rendering code is developed with Vulkan API.

Currently, the following demos are implemented through the real-time renderer:
  - SSAO
  - SPH simulation with compute shaders
  - Shadow maps (standard, PCF, variance, cascaded)
  - Simple physically-based shading model
  - Image-based lighting with split sum approximation
  - Screen-space reflections
  - Normal mapping
  - Simple point sprite rendering
  - Basic alpha masking for foliage
  - Tiled/clustered forward shading
  - Dynamic tessellation with terrain
  - Ocean FFT-based simulation
  - Atmospheric shading model based on UE4
  
The ray tracing section of the code (codenamed Vesper in the source) has the following features:
  - BRDFs
    - Lambertian
    - Oren Nayar
    - Rough & smooth conductor
    - Rough & smooth dielectric
    - Beckmann and GGX microfacet models
  - Integrators
    - Ambient occlusion
    - Direct lighting
    - Path tracing with MIS
  - Point, area, environment lighting
  
  Some pictures and more information are listed at the [project page](https://fallenshard.github.io/crisp-home.html)
