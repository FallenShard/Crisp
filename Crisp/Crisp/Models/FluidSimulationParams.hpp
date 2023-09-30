#pragma once

namespace crisp {
static constexpr float PI = 3.141592f;

static constexpr float vizScale = 10.0f;
static constexpr float particleRadius = 0.01f;
static constexpr float particleDiameter = 2.0f * particleRadius;
static constexpr float particleSpacing = particleDiameter;
static constexpr float particleVolume = particleDiameter * particleDiameter * particleDiameter;

static constexpr float h = 2.0f * particleSpacing;
static constexpr float h2 = h * h;
static constexpr float h3 = h2 * h;

static constexpr float poly6Const = 315.0f / (64.0f * PI * h3 * h3 * h3);

static constexpr float spikyGradConst = -45.0f / (PI * h2 * h2 * h2);

static constexpr float viscosityLaplaceConst = 45.0f / (PI * h3 * h3);

static constexpr float csConst = 3 / 2.0f / PI;

static constexpr float stiffness = 100.0f;
static constexpr float restDensity = 1000.0f;
static constexpr float mass = particleVolume * restDensity;

static constexpr float gX = 0.0f;
static constexpr float gY = -9.81f;
static constexpr float gZ = 0.0f;

static constexpr float timeStepFraction = 0.16f;
static constexpr int integrationSteps = 1;
} // namespace crisp