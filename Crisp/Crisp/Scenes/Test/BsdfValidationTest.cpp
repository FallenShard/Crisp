#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/ShaderUtils/Test/TestShaderMap.hpp>
#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <span>
#include <utility>

namespace crisp {
namespace {

using BsdfValidationTest = VulkanTest;

constexpr uint32_t kSampleCount = 1u << 14;
constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
constexpr float kMicrofacetAlpha = 0.3f;
const auto kShaderSourceDirectory = std::filesystem::path{"TestData"} / "CrispBsdfValidationTest";
const TestShaderMap kTestShaders{kShaderSourceDirectory / "bsdf-validation.comp.glsl"};

enum class Model : uint32_t { // NOLINT
    Lambertian,
    OrenNayar,
    Microfacet,
    DielectricFresnel,
    ConductorFresnel,
    SmoothConductor,
    MicrofacetNormal,
    RoughConductor,
    RoughDielectric,
};

enum class Operation : uint32_t { Evaluate, Sample, Limit, CriticalAngle }; // NOLINT
enum class MicrofacetType : int32_t { Ggx, Beckmann };       // NOLINT

struct PushConstants {
    uint32_t sampleCount;
    Model model;
    Operation operation;
    MicrofacetType microfacetType;
};

static_assert(sizeof(PushConstants) == 4 * sizeof(uint32_t));

struct ValidationResult {
    glm::vec4 wiAndAux;
    glm::vec4 woAndPdf;
    glm::vec4 value;
    glm::vec4 reverseValue;
};

static_assert(sizeof(ValidationResult) == 4 * sizeof(glm::vec4));

std::vector<ValidationResult> runValidationShader(
    VulkanDevice& device,
    const Model model,
    const Operation operation,
    const MicrofacetType microfacetType = MicrofacetType::Ggx) {
    constexpr VkExtent3D kWorkGroupSize{64, 1, 1};
    const VkDeviceSize byteSize = kSampleCount * sizeof(ValidationResult);
    VulkanBuffer resultBuffer(
        device,
        byteSize,
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT,
        BufferMemoryType::GpuOnly);
    VulkanBuffer readbackBuffer(device, byteSize, VK_BUFFER_USAGE_2_TRANSFER_DST_BIT, BufferMemoryType::HostReadback);

    auto pipeline =
        createComputePipeline(device, kTestShaders.getSpirvPath("bsdf-validation.comp.glsl"), kWorkGroupSize);
    Material material(pipeline.get());
    material.writeDescriptor(0, 0, resultBuffer.createDescriptorInfo());
    device.flushDescriptorUpdates();

    {
        const ScopeCommandExecutor executor(device);
        const auto& encoder = executor.cmdEncoder;
        encoder.bindPipeline(*pipeline);
        encoder.bindDescriptorSets(material.getDescriptorSetBinding());
        encoder.setPushConstants(
            *pipeline->getPipelineLayout(),
            VK_SHADER_STAGE_COMPUTE_BIT,
            PushConstants{kSampleCount, model, operation, microfacetType});
        encoder.dispatchCompute(computeWorkGroupCount(glm::uvec3{kSampleCount, 1, 1}, kWorkGroupSize));
        encoder.insertBufferMemoryBarrier(resultBuffer.createDescriptorInfo(), kComputeStorageWrite >> kTransferRead);
        encoder.copyBuffer(resultBuffer, readbackBuffer);
        encoder.insertBufferMemoryBarrier(readbackBuffer.createDescriptorInfo(), kTransferWrite >> kHostRead);
    }

    const auto* data = readbackBuffer.getHostVisibleData<ValidationResult>();
    return {data, data + kSampleCount}; // NOLINT
}

bool isFinite(const glm::vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool isFiniteAndNonNegative(const glm::vec3 value) {
    return isFinite(value) && glm::all(glm::greaterThanEqual(value, glm::vec3(0.0f)));
}

glm::vec3 integrateUniformHemisphere(
    const std::span<const ValidationResult> results, const uint32_t incidentDirectionIndex) {
    glm::dvec3 sum{0.0};
    size_t sampleCount = 0;
    for (size_t i = incidentDirectionIndex; i < results.size(); i += 4) {
        sum += glm::dvec3(results[i].value);
        ++sampleCount;
    }
    return glm::vec3(sum * (2.0 * std::numbers::pi / static_cast<double>(sampleCount))); // NOLINT
}

glm::dvec3 fresnelConductorReference(const double cosTheta, const glm::dvec3 eta, const glm::dvec3 k) {
    const double cosThetaSquared = cosTheta * cosTheta;
    const double sinThetaSquared = 1.0 - cosThetaSquared;
    const glm::dvec3 etaSquared = eta * eta;
    const glm::dvec3 kSquared = k * k;
    const glm::dvec3 t0 = etaSquared - kSquared - sinThetaSquared;
    const glm::dvec3 a2b2 = glm::sqrt(t0 * t0 + 4.0 * etaSquared * kSquared);
    const glm::dvec3 t1 = a2b2 + cosThetaSquared;
    const glm::dvec3 a = glm::sqrt(0.5 * (a2b2 + t0));
    const glm::dvec3 t2 = 2.0 * a * cosTheta;
    const glm::dvec3 rs = (t1 - t2) / (t1 + t2);
    const glm::dvec3 t3 = cosThetaSquared * a2b2 + sinThetaSquared * sinThetaSquared;
    const glm::dvec3 t4 = t2 * sinThetaSquared;
    const glm::dvec3 rp = rs * (t3 - t4) / (t3 + t4);
    return 0.5 * (rp + rs);
}

double fresnelDielectricReference(double cosThetaI, const double extIor, const double intIor) {
    double etaI = extIor;
    double etaT = intIor;
    if (cosThetaI < 0.0) {
        std::swap(etaI, etaT);
        cosThetaI = -cosThetaI;
    }
    const double eta = etaI / etaT;
    const double sinThetaTSquared = eta * eta * (1.0 - cosThetaI * cosThetaI);
    if (sinThetaTSquared > 1.0) {
        return 1.0;
    }
    const double cosThetaT = std::sqrt(1.0 - sinThetaTSquared);
    const double rs = (etaI * cosThetaI - etaT * cosThetaT) / (etaI * cosThetaI + etaT * cosThetaT);
    const double rp = (etaT * cosThetaI - etaI * cosThetaT) / (etaT * cosThetaI + etaI * cosThetaT);
    return 0.5 * (rs * rs + rp * rp);
}

TEST_F(BsdfValidationTest, ContinuousBsdfsAreFiniteNonNegativeAndReciprocal) {
    constexpr std::array configurations{
        std::pair{Model::Lambertian, MicrofacetType::Ggx},
        std::pair{Model::OrenNayar, MicrofacetType::Ggx},
        std::pair{Model::Microfacet, MicrofacetType::Ggx},
        std::pair{Model::Microfacet, MicrofacetType::Beckmann},
        std::pair{Model::RoughConductor, MicrofacetType::Ggx},
        std::pair{Model::RoughConductor, MicrofacetType::Beckmann},
    };
    for (const auto [model, microfacetType] : configurations) {
        const auto results = runValidationShader(*device_, model, Operation::Evaluate, microfacetType);
        for (size_t i = 0; i < results.size(); ++i) {
            SCOPED_TRACE(static_cast<uint32_t>(model));
            SCOPED_TRACE(i);
            const auto& result = results[i];
            const glm::vec3 wi = result.wiAndAux;
            const glm::vec3 wo = result.woAndPdf;
            ASSERT_TRUE(isFinite(wi));
            ASSERT_TRUE(isFinite(wo));
            ASSERT_GT(wi.z, 0.0f);
            ASSERT_GT(wo.z, 0.0f);
            EXPECT_TRUE(isFiniteAndNonNegative(glm::vec3(result.value)));
            EXPECT_TRUE(std::isfinite(result.woAndPdf.w));
            EXPECT_GE(result.woAndPdf.w, 0.0f);

            const glm::vec3 forwardBrdf = glm::vec3(result.value) / wo.z;
            const glm::vec3 reverseBrdf = glm::vec3(result.reverseValue) / wi.z;
            for (int32_t channel = 0; channel < 3; ++channel) {
                const float magnitude = std::max({1.0f, std::abs(forwardBrdf[channel]), std::abs(reverseBrdf[channel])});
                EXPECT_NEAR(forwardBrdf[channel], reverseBrdf[channel], 2e-6f + 2e-5f * magnitude);
            }
        }
    }
}

TEST_F(BsdfValidationTest, ContinuousBsdfsConserveEnergy) {
    constexpr std::array configurations{
        std::pair{Model::Lambertian, MicrofacetType::Ggx},
        std::pair{Model::OrenNayar, MicrofacetType::Ggx},
        std::pair{Model::Microfacet, MicrofacetType::Ggx},
        std::pair{Model::Microfacet, MicrofacetType::Beckmann},
        std::pair{Model::RoughConductor, MicrofacetType::Ggx},
        std::pair{Model::RoughConductor, MicrofacetType::Beckmann},
    };
    for (const auto [model, microfacetType] : configurations) {
        const auto results = runValidationShader(*device_, model, Operation::Evaluate, microfacetType);
        for (uint32_t incidentDirection = 0; incidentDirection < 4; ++incidentDirection) {
            SCOPED_TRACE(static_cast<uint32_t>(model));
            SCOPED_TRACE(incidentDirection);
            const glm::vec3 reflectance = integrateUniformHemisphere(results, incidentDirection);
            const glm::vec3 upperBound =
                model == Model::Lambertian || model == Model::OrenNayar ? glm::vec3{0.8f, 0.6f, 0.4f} : glm::vec3{1.0f};
            for (int32_t channel = 0; channel < 3; ++channel) {
                EXPECT_GE(reflectance[channel], 0.0f);
                EXPECT_LE(reflectance[channel], upperBound[channel] + 1.5e-2f);
            }
        }
    }
}

TEST_F(BsdfValidationTest, OrenNayarZeroRoughnessMatchesLambertian) {
    const auto lambertian = runValidationShader(*device_, Model::Lambertian, Operation::Evaluate);
    const auto orenNayar = runValidationShader(*device_, Model::OrenNayar, Operation::Limit);
    for (size_t i = 0; i < lambertian.size(); ++i) {
        SCOPED_TRACE(i);
        for (int32_t channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(orenNayar[i].value[channel], lambertian[i].value[channel], 2e-6f);
        }
    }
}

TEST_F(BsdfValidationTest, DiffusePdfNormalizesAndSamplingMatchesIt) {
    const auto evaluated = runValidationShader(*device_, Model::Lambertian, Operation::Evaluate);
    const auto sampled = runValidationShader(*device_, Model::Lambertian, Operation::Sample);
    double pdfIntegral = 0.0;
    constexpr uint32_t kPhiBinCount = 16;
    constexpr uint32_t kCosineBinCount = 8;
    std::array<uint32_t, kPhiBinCount * kCosineBinCount> observed{};

    for (size_t i = 0; i < sampled.size(); ++i) {
        pdfIntegral += evaluated[i].woAndPdf.w;
        const glm::vec3 direction = sampled[i].woAndPdf;
        ASSERT_TRUE(isFinite(direction));
        ASSERT_TRUE(std::isfinite(sampled[i].woAndPdf.w));
        ASSERT_GE(sampled[i].woAndPdf.w, 0.0f);
        ASSERT_GE(direction.z, 0.0f);
        ASSERT_NEAR(glm::dot(direction, direction), 1.0f, 2e-5f);
        ASSERT_NEAR(sampled[i].woAndPdf.w, direction.z / std::numbers::pi_v<float>, 2e-6f);

        float phi = std::atan2(direction.y, direction.x);
        if (phi < 0.0f) {
            phi += kTwoPi;
        }
        const uint32_t phiBin = std::min(static_cast<uint32_t>(phi / kTwoPi * kPhiBinCount), kPhiBinCount - 1);
        const uint32_t cosineBin = std::min(static_cast<uint32_t>(direction.z * kCosineBinCount), kCosineBinCount - 1);
        ++observed[cosineBin * kPhiBinCount + phiBin];
    }

    pdfIntegral *= kTwoPi / static_cast<double>(evaluated.size());
    EXPECT_NEAR(pdfIntegral, 1.0, 7e-3);

    double chiSquared = 0.0;
    for (uint32_t cosineBin = 0; cosineBin < kCosineBinCount; ++cosineBin) {
        const double z0 = static_cast<double>(cosineBin) / kCosineBinCount;
        const double z1 = static_cast<double>(cosineBin + 1) / kCosineBinCount;
        const double expected = sampled.size() * (z1 * z1 - z0 * z0) / kPhiBinCount;
        for (uint32_t phiBin = 0; phiBin < kPhiBinCount; ++phiBin) {
            const double difference = observed[cosineBin * kPhiBinCount + phiBin] - expected;
            chiSquared += difference * difference / expected;
        }
    }
    EXPECT_LT(chiSquared, 190.0);
}

TEST_F(BsdfValidationTest, BeckmannNormalDistributionNormalizesAndSamplingMatchesPdf) {
    const auto evaluated =
        runValidationShader(*device_, Model::MicrofacetNormal, Operation::Evaluate, MicrofacetType::Beckmann);
    const auto sampled =
        runValidationShader(*device_, Model::MicrofacetNormal, Operation::Sample, MicrofacetType::Beckmann);
    double pdfIntegral = 0.0;
    constexpr uint32_t kPhiBinCount = 16;
    constexpr uint32_t kCdfBinCount = 8;
    std::array<uint32_t, kPhiBinCount * kCdfBinCount> observed{};

    for (size_t i = 0; i < sampled.size(); ++i) {
        pdfIntegral += evaluated[i].woAndPdf.w;
        const glm::vec3 normal = sampled[i].woAndPdf;
        ASSERT_TRUE(isFinite(normal));
        ASSERT_GT(normal.z, 0.0f);
        ASSERT_NEAR(glm::dot(normal, normal), 1.0f, 2e-5f);
        ASSERT_NEAR(sampled[i].woAndPdf.w, sampled[i].value.x * normal.z, 2e-5f);

        float phi = std::atan2(normal.y, normal.x);
        if (phi < 0.0f) {
            phi += kTwoPi;
        }
        const float tanThetaSquared = (1.0f - normal.z * normal.z) / (normal.z * normal.z);
        const float cdf = std::exp(-tanThetaSquared / (kMicrofacetAlpha * kMicrofacetAlpha));
        const uint32_t phiBin = std::min(static_cast<uint32_t>(phi / kTwoPi * kPhiBinCount), kPhiBinCount - 1);
        const uint32_t cdfBin = std::min(static_cast<uint32_t>(cdf * kCdfBinCount), kCdfBinCount - 1);
        ++observed[cdfBin * kPhiBinCount + phiBin];
    }

    pdfIntegral *= kTwoPi / static_cast<double>(evaluated.size());
    EXPECT_NEAR(pdfIntegral, 1.0, 1e-2);

    const double expected = static_cast<double>(sampled.size()) / observed.size();
    double chiSquared = 0.0;
    for (const uint32_t count : observed) {
        const double difference = count - expected;
        chiSquared += difference * difference / expected;
    }
    EXPECT_LT(chiSquared, 190.0);
}

TEST_F(BsdfValidationTest, FresnelFunctionsMatchDoublePrecisionReferences) {
    const auto dielectric = runValidationShader(*device_, Model::DielectricFresnel, Operation::Evaluate);
    const auto conductor = runValidationShader(*device_, Model::ConductorFresnel, Operation::Evaluate);
    constexpr glm::dvec3 kGoldEta{0.1431189557, 0.3749570432, 1.4424785571};
    constexpr glm::dvec3 kGoldK{3.9831604247, 2.3857207478, 1.6032152899};

    for (size_t i = 0; i < dielectric.size(); ++i) {
        SCOPED_TRACE(i);
        const double cosTheta = dielectric[i].wiAndAux.w;
        EXPECT_NEAR(dielectric[i].value.x, fresnelDielectricReference(cosTheta, 1.0, 1.5046), 2e-5);
        EXPECT_NEAR(dielectric[i].value.y, fresnelDielectricReference(-cosTheta, 1.0, 1.5046), 2e-5);
        EXPECT_GE(dielectric[i].value.x, 0.0f);
        EXPECT_LE(dielectric[i].value.x, 1.0f);
        EXPECT_GE(dielectric[i].value.y, 0.0f);
        EXPECT_LE(dielectric[i].value.y, 1.0f);

        const glm::dvec3 reference = fresnelConductorReference(conductor[i].wiAndAux.w, kGoldEta, kGoldK);
        for (int32_t channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(conductor[i].value[channel], reference[channel], 2e-5);
            EXPECT_GE(conductor[i].value[channel], 0.0f);
            EXPECT_LE(conductor[i].value[channel], 1.0f);
        }
    }
}

TEST_F(BsdfValidationTest, SmoothConductorSatisfiesDeltaContract) {
    const auto results = runValidationShader(*device_, Model::SmoothConductor, Operation::Sample);
    for (size_t i = 0; i < results.size(); ++i) {
        SCOPED_TRACE(i);
        const glm::vec3 wi = results[i].wiAndAux;
        const glm::vec3 expectedWo{-wi.x, -wi.y, wi.z};
        const glm::vec3 actualWo = results[i].woAndPdf;
        EXPECT_TRUE(isFinite(actualWo));
        EXPECT_NEAR(glm::dot(actualWo, actualWo), 1.0f, 2e-5f);
        for (int32_t axis = 0; axis < 3; ++axis) {
            EXPECT_NEAR(actualWo[axis], expectedWo[axis], 2e-6f);
        }
        EXPECT_FLOAT_EQ(results[i].woAndPdf.w, 1.0f);

        const glm::dvec3 reference = fresnelConductorReference(
            wi.z,
            glm::dvec3{0.1431189557, 0.3749570432, 1.4424785571},
            glm::dvec3{3.9831604247, 2.3857207478, 1.6032152899});
        EXPECT_TRUE(isFiniteAndNonNegative(glm::vec3(results[i].value)));
        for (int32_t channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(results[i].value[channel], reference[channel], 2e-5);
        }
    }
}

TEST_F(BsdfValidationTest, RoughDielectricIsFiniteConservativeAndEtaReciprocal) {
    constexpr float kExtIor = 1.0f;
    constexpr float kIntIor = 1.5046f;
    constexpr float kEtaItSquared = (kExtIor / kIntIor) * (kExtIor / kIntIor);
    constexpr std::array microfacetTypes{MicrofacetType::Ggx, MicrofacetType::Beckmann};

    for (const MicrofacetType microfacetType : microfacetTypes) {
        const auto results = runValidationShader(*device_, Model::RoughDielectric, Operation::Evaluate, microfacetType);
        std::array<glm::dvec3, 4> flux{};
        std::array<double, 4> pdfMass{};
        std::array<size_t, 4> counts{};

        for (size_t i = 0; i < results.size(); ++i) {
            SCOPED_TRACE(static_cast<int32_t>(microfacetType));
            SCOPED_TRACE(i);
            const auto& result = results[i];
            const glm::vec3 wi = result.wiAndAux;
            const glm::vec3 wo = result.woAndPdf;
            const glm::vec3 value = result.value;
            const glm::vec3 reverseValue = result.reverseValue;
            ASSERT_TRUE(isFinite(wi));
            ASSERT_TRUE(isFinite(wo));
            EXPECT_TRUE(isFiniteAndNonNegative(value));
            EXPECT_TRUE(isFiniteAndNonNegative(reverseValue));
            EXPECT_TRUE(std::isfinite(result.woAndPdf.w));
            EXPECT_GE(result.woAndPdf.w, 0.0f);

            if (glm::any(glm::greaterThan(value, glm::vec3(0.0f)))) {
                const glm::vec3 forward = value / std::abs(wo.z);
                const glm::vec3 reverse = reverseValue / wi.z;
                const bool reflection = wo.z > 0.0f;
                for (int32_t channel = 0; channel < 3; ++channel) {
                    const float expectedForward = reflection ? reverse[channel] : reverse[channel] * kEtaItSquared;
                    const float magnitude = std::max({1.0f, std::abs(forward[channel]), std::abs(expectedForward)});
                    EXPECT_NEAR(forward[channel], expectedForward, 3e-6f + 4e-5f * magnitude);
                }
            }

            const uint32_t incidentDirection = static_cast<uint32_t>(i & 3u);
            const double fluxScale = wo.z < 0.0f ? 1.0 / kEtaItSquared : 1.0;
            flux[incidentDirection] += glm::dvec3(value) * fluxScale;
            pdfMass[incidentDirection] += result.woAndPdf.w;
            ++counts[incidentDirection];
        }

        for (uint32_t incidentDirection = 0; incidentDirection < 4; ++incidentDirection) {
            SCOPED_TRACE(incidentDirection);
            const double integrationWeight = 4.0 * std::numbers::pi / static_cast<double>(counts[incidentDirection]);
            const glm::dvec3 integratedFlux = flux[incidentDirection] * integrationWeight;
            for (int32_t channel = 0; channel < 3; ++channel) {
                EXPECT_GE(integratedFlux[channel], 0.0);
                EXPECT_LE(integratedFlux[channel], 1.04);
            }
            const double integratedPdf = pdfMass[incidentDirection] * integrationWeight;
            EXPECT_GT(integratedPdf, 0.0);
            EXPECT_LE(integratedPdf, 1.04);
        }
    }
}

TEST_F(BsdfValidationTest, RoughDielectricSamplingMatchesEvaluationAndFresnelBranches) {
    constexpr float kExtIor = 1.0f;
    constexpr float kIntIor = 1.5046f;
    constexpr std::array microfacetTypes{MicrofacetType::Ggx, MicrofacetType::Beckmann};

    for (const MicrofacetType microfacetType : microfacetTypes) {
        const auto results = runValidationShader(*device_, Model::RoughDielectric, Operation::Sample, microfacetType);
        double expectedReflectionCount = 0.0;
        double reflectionVariance = 0.0;
        size_t observedReflectionCount = 0;
        size_t validNormalCount = 0;

        for (size_t i = 0; i < results.size(); ++i) {
            SCOPED_TRACE(static_cast<int32_t>(microfacetType));
            SCOPED_TRACE(i);
            const auto& result = results[i];
            const glm::vec3 wi = result.wiAndAux;
            const glm::vec3 wo = result.woAndPdf;
            const glm::vec3 evaluatedF = result.value;
            const glm::vec3 sampledF = result.reverseValue;
            const float fresnel = result.wiAndAux.w;

            if (fresnel >= 0.0f) {
                ++validNormalCount;
                expectedReflectionCount += fresnel;
                reflectionVariance += fresnel * (1.0 - fresnel);
                observedReflectionCount += result.reverseValue.w >= 0.0f ? 1u : 0u;
            }

            EXPECT_TRUE(isFiniteAndNonNegative(evaluatedF));
            EXPECT_TRUE(isFiniteAndNonNegative(sampledF));
            EXPECT_TRUE(std::isfinite(result.woAndPdf.w));
            EXPECT_TRUE(std::isfinite(result.value.w));
            EXPECT_GE(result.woAndPdf.w, 0.0f);
            EXPECT_GE(result.value.w, 0.0f);
            EXPECT_NEAR(result.woAndPdf.w, result.value.w, 2e-5f);
            for (int32_t channel = 0; channel < 3; ++channel) {
                EXPECT_NEAR(sampledF[channel], evaluatedF[channel], 2e-5f);
            }

            if (result.woAndPdf.w == 0.0f) {
                EXPECT_EQ(wo, glm::vec3(0.0f));
                continue;
            }

            EXPECT_NEAR(glm::dot(wo, wo), 1.0f, 3e-5f);
            const bool reflection = wi.z * wo.z > 0.0f;
            EXPECT_EQ(reflection, result.reverseValue.w >= 0.0f);

            if (!reflection) {
                const glm::vec3 halfVector = glm::normalize(wi + wo * (kIntIor / kExtIor));
                const glm::vec3 microfacetNormal = halfVector.z < 0.0f ? -halfVector : halfVector;
                const double sinThetaI = std::sqrt(std::max(0.0, 1.0 - std::pow(glm::dot(wi, microfacetNormal), 2.0f)));
                const double sinThetaO = std::sqrt(std::max(0.0, 1.0 - std::pow(glm::dot(wo, microfacetNormal), 2.0f)));
                EXPECT_NEAR(kExtIor * sinThetaI, kIntIor * sinThetaO, 2e-4);
            }
        }

        ASSERT_GT(validNormalCount, 0u);
        const double standardDeviation = std::sqrt(reflectionVariance);
        EXPECT_NEAR(
            static_cast<double>(observedReflectionCount),
            expectedReflectionCount,
            std::max(8.0, 5.0 * standardDeviation));
    }
}

TEST_F(BsdfValidationTest, RoughDielectricPdfIncludesItsNullEventMass) {
    constexpr std::array microfacetTypes{MicrofacetType::Ggx, MicrofacetType::Beckmann};
    for (const MicrofacetType microfacetType : microfacetTypes) {
        const auto sampled = runValidationShader(*device_, Model::RoughDielectric, Operation::Sample, microfacetType);
        std::array<double, 4> expectedDirectionalMass{};
        std::array<size_t, 4> validSamples{};
        std::array<size_t, 4> counts{};

        for (size_t i = 0; i < sampled.size(); ++i) {
            const uint32_t incidentDirection = static_cast<uint32_t>(i & 3u);
            expectedDirectionalMass[incidentDirection] += std::abs(sampled[i].reverseValue.w);
            validSamples[incidentDirection] += sampled[i].woAndPdf.w > 0.0f ? 1u : 0u;
            ++counts[incidentDirection];
        }

        for (uint32_t incidentDirection = 0; incidentDirection < 4; ++incidentDirection) {
            SCOPED_TRACE(static_cast<int32_t>(microfacetType));
            SCOPED_TRACE(incidentDirection);
            const double integratedPdf =
                expectedDirectionalMass[incidentDirection] / static_cast<double>(counts[incidentDirection]);
            const double observedDirectionalMass =
                static_cast<double>(validSamples[incidentDirection]) / static_cast<double>(counts[incidentDirection]);
            EXPECT_NEAR(integratedPdf, observedDirectionalMass, 2e-2);
        }
    }
}

TEST_F(BsdfValidationTest, RoughDielectricHandlesCriticalAngleAndTotalInternalReflection) {
    const auto results =
        runValidationShader(*device_, Model::RoughDielectric, Operation::CriticalAngle, MicrofacetType::Beckmann);
    for (size_t i = 0; i < results.size(); ++i) {
        SCOPED_TRACE(i);
        const glm::vec3 wi = results[i].wiAndAux;
        const glm::vec3 wo = results[i].woAndPdf;
        ASSERT_LT(wi.z, 0.0f);
        ASSERT_GT(results[i].woAndPdf.w, 0.0f);
        EXPECT_NEAR(glm::dot(wo, wo), 1.0f, 3e-5f);
        if ((i & 1u) == 0u) {
            EXPECT_GT(wo.z, 0.0f) << "below the critical angle must transmit";
        } else {
            EXPECT_FLOAT_EQ(results[i].wiAndAux.w, 1.0f);
            EXPECT_LT(wo.z, 0.0f) << "above the critical angle must reflect via TIR";
            EXPECT_FLOAT_EQ(results[i].reverseValue.w, 1.0f);
        }
    }
}

} // namespace
} // namespace crisp
