# Crisp

![crisp-banner](https://user-images.githubusercontent.com/5392742/147391331-3e1fee1c-9f5b-4696-96b2-efcbb2b63c0c.png)

This project contains a small application that showcases reference implementations of various algorithms for ray tracing and
real-time rendering that I have encountered in computer graphics, both during my studies and in my career.

It is developed sporadically during my free time so updates are not very frequent. The real-time rendering code is developed with Vulkan API.

It is developed sporadically during my free time so updates are not very frequent.
The real-time rendering code is developed with Vulkan API.

Currently, the following demos are implemented through the real-time renderer:
  - SSAO
  - (WC)SPH simulation with compute shaders
  - Shadow maps (standard, PCF, variance, cascaded)
  - Simple physically-based shading model based on UE4-style metallic workflow
  - Image-based lighting with split sum approximation
  - Screen-space reflections
  - Simple point sprite rendering
  - Basic alpha masking for foliage
  - Tiled forward shading demo with thousands of lights
  - Dynamic terrain tessellation
  - Atmospheric shading model based on https://sebh.github.io/publications/egsr2020.pdf
  - FFT-based ocean simulation
  
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
