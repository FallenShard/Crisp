add_cpp_static_library(PathTracerBSDF
    "BSDFs/BSDF.cpp"
    "BSDFs/BSDF.hpp"
    "BSDFs/BSDFFactory.cpp"
    "BSDFs/BSDFFactory.hpp"
    "BSDFs/DielectricBSDF.cpp"
    "BSDFs/DielectricBSDF.hpp"
    "BSDFs/LambertianBSDF.cpp"
    "BSDFs/LambertianBSDF.hpp"
    "BSDFs/Microfacet.cpp"
    "BSDFs/Microfacet.hpp"
    "BSDFs/MicrofacetDistributions/Beckmann.cpp"
    "BSDFs/MicrofacetDistributions/Beckmann.hpp"
    "BSDFs/MicrofacetDistributions/GGX.cpp"
    "BSDFs/MicrofacetDistributions/GGX.hpp"
    "BSDFs/MicrofacetDistributions/MicrofacetDistribution.hpp"
    "BSDFs/MicrofacetDistributions/MicrofacetDistributionFactory.cpp"
    "BSDFs/MicrofacetDistributions/MicrofacetDistributionFactory.hpp"
    "BSDFs/MicrofacetDistributions/Phong.cpp"
    "BSDFs/MicrofacetDistributions/Phong.hpp"
    "BSDFs/Mirror.cpp"
    "BSDFs/Mirror.hpp"
    "BSDFs/OrenNayar.cpp"
    "BSDFs/OrenNayar.hpp"
    "BSDFs/RoughConductor.cpp"
    "BSDFs/RoughConductor.hpp"
    "BSDFs/RoughDielectric.cpp"
    "BSDFs/RoughDielectric.hpp"
    "BSDFs/SmoothConductor.cpp"
    "BSDFs/SmoothConductor.hpp"
)
target_link_libraries(PathTracerBSDF
    PUBLIC PathTracerUtils
    PUBLIC PathTracerTextures
    PUBLIC Crisp::Optics
    PRIVATE PathTracerSamplers
)

add_cpp_static_library(PathTracerBSSRDF
    "BSSRDFs/BSSRDF.hpp"
    "BSSRDFs/BSSRDFFactory.cpp"
    "BSSRDFs/BSSRDFFactory.hpp"
    "BSSRDFs/DipoleBSSRDF.cpp"
    "BSSRDFs/DipoleBSSRDF.hpp"
    "BSSRDFs/IrradianceTree.hpp"
)
target_link_libraries(PathTracerBSSRDF
    PUBLIC PathTracerUtils
    PUBLIC PathTracerTextures
    PUBLIC Crisp::Optics
    PUBLIC embree
    PRIVATE PathTracerSamplers
)

add_cpp_static_library(PathTracerCamera
    "Cameras/Camera.cpp"
    "Cameras/Camera.hpp"
    "Cameras/CameraFactory.cpp"
    "Cameras/CameraFactory.hpp"
    "Cameras/Perspective.cpp"
    "Cameras/Perspective.hpp"
)
target_link_libraries(PathTracerCamera
    PUBLIC PathTracerUtils
    PUBLIC PathTracerFilters
    PRIVATE PathTracerSamplers
)

add_cpp_static_library(PathTracerUtils
    "Core/VariantMap.hpp"
    "Core/Intersection.hpp"
    "Core/MipMap.cpp"
    "Core/MipMap.hpp"
)
target_link_libraries(PathTracerUtils
    PUBLIC Crisp::Math
    PUBLIC Crisp::HashMap
    PUBLIC Crisp::Spectrum
    PRIVATE Crisp::ImageIo
)

add_cpp_static_library(PathTracerIntegrator
    "Integrators/AmbientOcclusion.cpp"
    "Integrators/AmbientOcclusion.hpp"
    "Integrators/DirectLighting.cpp"
    "Integrators/DirectLighting.hpp"
    "Integrators/EmsDirectLighting.cpp"
    "Integrators/EmsDirectLighting.hpp"
    "Integrators/Integrator.hpp"
    "Integrators/IntegratorFactory.cpp"
    "Integrators/IntegratorFactory.hpp"
    "Integrators/MatsDirectLighting.cpp"
    "Integrators/MatsDirectLighting.hpp"
    "Integrators/MisDirectLighting.cpp"
    "Integrators/MisDirectLighting.hpp"
    "Integrators/MisPathTracer.cpp"
    "Integrators/MisPathTracer.hpp"
    "Integrators/Normals.cpp"
    "Integrators/Normals.hpp"
    "Integrators/PathTracer.cpp"
    "Integrators/PathTracer.hpp"
    "Integrators/VolumePathTracer.cpp"
    "Integrators/VolumePathTracer.hpp"
)
target_link_libraries(PathTracerIntegrator
    PUBLIC PathTracerUtils
    PUBLIC Crisp::Utils
    PUBLIC Crisp::HashMap
    PUBLIC Crisp::Spectrum
    PRIVATE Crisp::PathTracer
    PRIVATE PathTracerLights
    PRIVATE PathTracerBSDF
    PRIVATE PathTracerSamplers
)

add_cpp_static_library(PathTracerLights
    "Lights/AreaLight.cpp"
    "Lights/AreaLight.hpp"
    "Lights/DirectionalLight.cpp"
    "Lights/DirectionalLight.hpp"
    "Lights/EnvironmentLight.cpp"
    "Lights/EnvironmentLight.hpp"
    "Lights/Light.hpp"
    "Lights/LightFactory.cpp"
    "Lights/LightFactory.hpp"
    "Lights/PointLight.cpp"
    "Lights/PointLight.hpp"
)
target_link_libraries(PathTracerLights
    PUBLIC PathTracerUtils
    PUBLIC PathTracerShapes
    PRIVATE PathTracerSamplers
)

add_cpp_static_library(PathTracerParticipatingMedia
    "Media/Homogeneous.cpp"
    "Media/Homogeneous.hpp"
    "Media/Medium.cpp"
    "Media/Medium.hpp"
    "Media/MediumFactory.cpp"
    "Media/MediumFactory.hpp"
    "PhaseFunctions/Isotropic.cpp"
    "PhaseFunctions/Isotropic.hpp"
    "PhaseFunctions/PhaseFunction.hpp"
    "PhaseFunctions/PhaseFunctionFactory.cpp"
    "PhaseFunctions/PhaseFunctionFactory.hpp"
)
target_link_libraries(PathTracerParticipatingMedia
    PUBLIC PathTracerUtils
    PRIVATE PathTracerSamplers
)

add_cpp_static_library(PathTracerFilters
    "ReconstructionFilters/GaussianFilter.cpp"
    "ReconstructionFilters/GaussianFilter.hpp"
    "ReconstructionFilters/ReconstructionFilter.cpp"
    "ReconstructionFilters/ReconstructionFilter.hpp"
)

add_cpp_static_library(PathTracerSamplers
    "Samplers/Fixed.cpp"
    "Samplers/Fixed.hpp"
    "Samplers/Independent.cpp"
    "Samplers/Independent.hpp"
    "Samplers/Sampler.cpp"
    "Samplers/Sampler.hpp"
    "Samplers/SamplerFactory.cpp"
    "Samplers/SamplerFactory.hpp"
)
target_link_libraries(PathTracerSamplers
    PUBLIC PathTracerUtils
)

add_cpp_static_library(PathTracerShapes
    "Shapes/Mesh.cpp"
    "Shapes/Mesh.hpp"
    "Shapes/MeshLoader.cpp"
    "Shapes/MeshLoader.hpp"
    "Shapes/Shape.cpp"
    "Shapes/Shape.hpp"
    "Shapes/ShapeFactory.cpp"
    "Shapes/ShapeFactory.hpp"
    "Shapes/Sphere.cpp"
    "Shapes/Sphere.hpp"
)
target_link_libraries(PathTracerShapes
    PUBLIC PathTracerUtils
    PRIVATE Crisp::Logger
    PUBLIC embree
)

add_cpp_static_library(PathTracerTextures
    "Textures/CheckerboardTexture.hpp"
    "Textures/ConstantTexture.hpp"
    "Textures/Texture.hpp"
    "Textures/TextureFactory.hpp"
    "Textures/UVTexture.cpp"
    "Textures/UVTexture.hpp"
)
target_link_libraries(PathTracerTextures
    PUBLIC PathTracerUtils
)

add_cpp_static_library(CrispPathTracer
    "Core/Scene.cpp"
    "Core/Scene.hpp"
    "Core/XmlSceneParser.cpp"
    "Core/XmlSceneParser.hpp"
    "ImageBlock.cpp"
    "ImageBlock.hpp"
    "RayTracer.cpp"
    "RayTracer.hpp"
    "RayTracerUpdate.hpp"
)
target_link_libraries(CrispPathTracer
    PUBLIC PathTracerBSDF
    PUBLIC PathTracerBSSRDF
    PUBLIC PathTracerFilters
    PUBLIC PathTracerIntegrator
    PUBLIC PathTracerCamera
    PUBLIC PathTracerShapes
    PUBLIC PathTracerLights
    PUBLIC PathTracerUtils
    PUBLIC embree
    PUBLIC tbb
    PUBLIC rapidxml
    PRIVATE Crisp::Logger
    PRIVATE Crisp::ImageIo
)
