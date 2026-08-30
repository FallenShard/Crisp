#include <Crisp/Math/Distribution2D.hpp>

#include <algorithm>

namespace crisp {
namespace {

uint32_t sampleCdf(const std::vector<float>& cdf, const uint32_t offset, const uint32_t count, float& sampleValue) {
    const auto begin = cdf.begin() + offset;
    const auto end = begin + count + 1;
    const auto upper = std::upper_bound(begin, end, sampleValue);
    const auto index = static_cast<uint32_t>(upper - begin - 1);

    const float intervalStart = cdf[offset + index];
    const float intervalEnd = cdf[offset + index + 1];
    sampleValue = (sampleValue - intervalStart) / (intervalEnd - intervalStart);
    return index;
}

} // namespace

Distribution2D::Distribution2D(const std::span<const float> weights, const uint32_t columnCount, const uint32_t rowCount)
    : m_columnCount(columnCount)
    , m_rowCount(rowCount) {
    m_cdf.assign(
        static_cast<size_t>(rowCount) + 1 + static_cast<size_t>(rowCount) * (static_cast<size_t>(columnCount) + 1),
        0.0f);
    std::vector<double> rowWeights(rowCount, 0.0);
    for (uint32_t row = 0; row < rowCount; ++row) {
        const uint32_t offset = getColumnCdfOffset(row);
        double rowWeight = 0.0;
        for (uint32_t column = 0; column < columnCount; ++column) {
            const float weight = weights[static_cast<size_t>(row) * columnCount + column];
            rowWeight += weight;
            m_cdf[offset + column + 1] = static_cast<float>(rowWeight);
        }

        if (rowWeight > 0.0) {
            const double inverseRowWeight = 1.0 / rowWeight;
            for (uint32_t column = 1; column < columnCount; ++column) {
                m_cdf[offset + column] = static_cast<float>(m_cdf[offset + column] * inverseRowWeight);
            }
        } else {
            for (uint32_t column = 1; column < columnCount; ++column) {
                m_cdf[offset + column] = static_cast<float>(column) / static_cast<float>(columnCount);
            }
        }
        m_cdf[offset + columnCount] = 1.0f;
        rowWeights[row] = rowWeight;
        m_weightSum += rowWeight;
    }

    if (m_weightSum == 0.0) {
        std::ranges::fill(rowWeights, 1.0);
    }

    double marginalSum = 0.0;
    for (const double rowWeight : rowWeights) {
        marginalSum += rowWeight;
    }
    double cumulative = 0.0;
    for (uint32_t row = 0; row < rowCount; ++row) {
        cumulative += rowWeights[row] / marginalSum;
        m_cdf[row + 1] = static_cast<float>(cumulative);
    }
    m_cdf[rowCount] = 1.0f;
}

Distribution2D::Distribution2D(const std::vector<std::vector<float>>& weights) {
    const size_t columnCount = weights.front().size();

    std::vector<float> flattened;
    flattened.reserve(weights.size() * columnCount);
    for (const auto& row : weights) {
        flattened.insert(flattened.end(), row.begin(), row.end());
    }
    *this = Distribution2D(flattened, static_cast<uint32_t>(columnCount), static_cast<uint32_t>(weights.size()));
}

bool Distribution2D::isEmpty() const {
    return m_cdf.empty();
}

uint32_t Distribution2D::getColumnCount() const {
    return m_columnCount;
}

uint32_t Distribution2D::getRowCount() const {
    return m_rowCount;
}

double Distribution2D::getWeightSum() const {
    return m_weightSum;
}

Distribution2D::Sample Distribution2D::sampleContinuous(glm::vec2 sampleValue) const {
    const uint32_t row = sampleCdf(m_cdf, 0, m_rowCount, sampleValue.y);
    const uint32_t column = sampleCdf(m_cdf, getColumnCdfOffset(row), m_columnCount, sampleValue.x);
    const glm::uvec2 cell{column, row};
    return {
        .value = (glm::vec2(cell) + sampleValue) / glm::vec2(m_columnCount, m_rowCount),
        .cell = cell,
        .pdf = getCellProbability(column, row) * static_cast<float>(m_columnCount) * static_cast<float>(m_rowCount),
    };
}

float Distribution2D::getPdf(const glm::vec2 value) const {
    const auto column = static_cast<uint32_t>(value.x * static_cast<float>(m_columnCount));
    const auto row = static_cast<uint32_t>(value.y * static_cast<float>(m_rowCount));
    return getCellProbability(column, row) * static_cast<float>(m_columnCount) * static_cast<float>(m_rowCount);
}

float Distribution2D::getCellProbability(const uint32_t column, const uint32_t row) const {
    const uint32_t offset = getColumnCdfOffset(row);
    const float rowProbability = m_cdf[row + 1] - m_cdf[row];
    const float columnProbability = m_cdf[offset + column + 1] - m_cdf[offset + column];
    return rowProbability * columnProbability;
}

const std::vector<float>& Distribution2D::getCdf() const {
    return m_cdf;
}

uint32_t Distribution2D::getColumnCdfOffset(const uint32_t row) const {
    return m_rowCount + 1 + row * (m_columnCount + 1);
}

} // namespace crisp
