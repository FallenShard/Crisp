#include <Crisp/Math/Constants.hpp>
#include <Crisp/PathTracer/BSDFs/BSDFFactory.hpp>
#include <Crisp/PathTracer/BSDFs/LambertianBSDF.hpp>
#include <Crisp/PathTracer/BSDFs/OrenNayar.hpp>
#include <Crisp/PathTracer/Samplers/Fixed.hpp>
#include <Crisp/PathTracer/Textures/ConstantTexture.hpp>

#include <gtest/gtest.h>

namespace crisp::test {
namespace {
void expectSpectrumNear(const Spectrum& actual, const Spectrum& expected, const float tolerance = 1.0e-6f) {
    EXPECT_NEAR(actual.r, expected.r, tolerance);
    EXPECT_NEAR(actual.g, expected.g, tolerance);
    EXPECT_NEAR(actual.b, expected.b, tolerance);
}

VariantMap createParameters(const Spectrum reflectance, const float roughnessDegrees) {
    VariantMap parameters;
    parameters.insert("reflectance", reflectance);
    parameters.insert("roughnessDegrees", roughnessDegrees);
    return parameters;
}

TEST(OrenNayarTest, FactoryCreatesOrenNayarBsdf) {
    auto bsdf = BSDFFactory::create("oren-nayar", createParameters(Spectrum(0.5f), 30.0f));

    EXPECT_NE(dynamic_cast<OrenNayarBSDF*>(bsdf.get()), nullptr);
}

TEST(OrenNayarTest, ZeroRoughnessMatchesLambertian) {
    const auto parameters = createParameters(Spectrum(0.2f, 0.4f, 0.8f), 0.0f);
    const OrenNayarBSDF orenNayar(parameters);
    const LambertianBSDF lambertian(parameters);

    BSDF::Sample sample(
        glm::vec3(0.0f),
        glm::vec2(0.25f, 0.75f),
        glm::normalize(glm::vec3(0.3f, 0.4f, 1.0f)),
        glm::normalize(glm::vec3(-0.2f, 0.1f, 1.0f)));
    sample.measure = BSDF::Measure::SolidAngle;

    expectSpectrumNear(orenNayar.eval(sample), lambertian.eval(sample));
    EXPECT_NEAR(orenNayar.pdf(sample), lambertian.pdf(sample), 1.0e-6f);
}

TEST(OrenNayarTest, RoughnessReducesNormalIncidenceReflectance) {
    const OrenNayarBSDF smooth(createParameters(Spectrum(1.0f), 0.0f));
    const OrenNayarBSDF rough(createParameters(Spectrum(1.0f), 45.0f));
    BSDF::Sample sample(glm::vec3(0.0f), glm::vec2(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    sample.measure = BSDF::Measure::SolidAngle;

    EXPECT_LT(rough.eval(sample).r, smooth.eval(sample).r);
}

TEST(OrenNayarTest, ReflectanceTextureOverridesConstantReflectance) {
    OrenNayarBSDF orenNayar(createParameters(Spectrum(0.0f), 0.0f));
    VariantMap textureParameters;
    textureParameters.insert("value", Spectrum(0.2f, 0.4f, 0.8f));
    orenNayar.setTexture(std::make_shared<ConstantTexture<Spectrum>>(textureParameters));

    BSDF::Sample sample(glm::vec3(0.0f), glm::vec2(0.5f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    sample.measure = BSDF::Measure::SolidAngle;

    expectSpectrumNear(orenNayar.eval(sample), Spectrum(0.2f, 0.4f, 0.8f) * InvPI<>);
}

TEST(OrenNayarTest, SampleReturnsEvalOverPdf) {
    OrenNayarBSDF orenNayar(createParameters(Spectrum(0.3f, 0.6f, 0.9f), 45.0f));
    FixedSampler sampler;
    BSDF::Sample sample(glm::vec3(0.0f), glm::vec2(0.5f), glm::normalize(glm::vec3(0.4f, 0.2f, 1.0f)));

    const Spectrum weight = orenNayar.sample(sample, sampler);

    EXPECT_EQ(sample.measure, BSDF::Measure::SolidAngle);
    EXPECT_EQ(sample.sampledLobe, Lobe::Diffuse);
    EXPECT_NEAR(sample.pdf, orenNayar.pdf(sample), 1.0e-6f);
    expectSpectrumNear(weight, orenNayar.eval(sample) / sample.pdf);
}
} // namespace
} // namespace crisp::test
