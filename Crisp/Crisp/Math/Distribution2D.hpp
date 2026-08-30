#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <Crisp/Math/Headers.hpp>

namespace crisp {

// A normalized piecewise-constant 2D probability distribution over the unit square, built from row-major
// non-negative weights. Sampling first selects a row from the marginal CDF, then a column from that row's
// conditional CDF, and remaps both residuals continuously within the selected cell.
//
// The packed CDF layout is intentionally uploadable as-is: rowCount + 1 marginal values followed by one
// columnCount + 1 conditional CDF per row.
class Distribution2D {
public:
    struct Sample {
        glm::vec2 value{0.0f};
        glm::uvec2 cell{0};
        float pdf{0.0f}; // Density with respect to area on the unit square.
    };

    Distribution2D() = default;
    // Precondition: dimensions are non-zero, packed offsets fit uint32_t, and weights has their finite non-negative product.
    Distribution2D(std::span<const float> weights, uint32_t columnCount, uint32_t rowCount);
    // Precondition: weights is a non-empty rectangular finite non-negative grid whose dimensions and packed offsets fit uint32_t.
    explicit Distribution2D(const std::vector<std::vector<float>>& weights);

    bool isEmpty() const;
    uint32_t getColumnCount() const;
    uint32_t getRowCount() const;
    double getWeightSum() const;

    // Precondition: non-empty distribution; both sampleValue components are finite and in [0, 1).
    Sample sampleContinuous(glm::vec2 sampleValue) const;
    // Precondition: non-empty distribution; both value components are finite and in [0, 1).
    float getPdf(glm::vec2 value) const;
    // Precondition: column < getColumnCount() and row < getRowCount().
    float getCellProbability(uint32_t column, uint32_t row) const;

    const std::vector<float>& getCdf() const;

private:
    uint32_t getColumnCdfOffset(uint32_t row) const;

    uint32_t m_columnCount{0};
    uint32_t m_rowCount{0};
    double m_weightSum{0.0};
    std::vector<float> m_cdf;
};

} // namespace crisp
