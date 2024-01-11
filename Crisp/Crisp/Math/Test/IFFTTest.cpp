#include <gtest/gtest.h>

#include <complex>
#include <random>

#include <Crisp/Math/Headers.hpp>

namespace crisp {
constexpr size_t Size = 8;

std::vector<std::complex<float>> toComplex(const std::vector<float>& real) {
    std::vector<std::complex<float>> complex(real.size());
    for (size_t i = 0; i < real.size(); ++i) {
        complex[i] = std::complex(real[i], 0.0f);
    }
    return complex;
}

uint32_t reverseBits(uint32_t k, int N) {
    uint32_t r = 0;
    while (N > 0) {
        r = (r << 1) | (k & 1);
        k >>= 1;
        N--;
    }

    return r;
}

std::vector<std::complex<float>> bitReverse(const std::vector<std::complex<float>>& signal) {
    int logN = (int)std::log2(signal.size());
    std::vector<std::complex<float>> result(signal);
    for (int i = 0; i < result.size(); ++i) {
        result[reverseBits(i, logN)] = signal[i];
    }
    return result;
}

std::vector<std::complex<float>> fft(const std::vector<std::complex<float>>& signal) {
    constexpr std::complex<float> im(0, 1);

    auto bitReversed = bitReverse(signal);
    int logN = (int)std::log2(signal.size());
    for (int s = 1; s <= logN; ++s) {
        int m = 1 << s;
        std::complex<float> omega_m = std::exp(-2.0f * glm::pi<float>() / m * im);
        for (int k = 0; k < signal.size(); k += m) {
            std::complex<float> omega = 1;
            for (int j = 0; j < m / 2; ++j) {
                std::complex t = omega * bitReversed[k + j + m / 2];
                std::complex u = bitReversed[k + j];
                bitReversed[k + j] = u + t;
                bitReversed[k + j + m / 2] = u - t;
                omega = omega * omega_m;
            }
        }
    }
    return bitReversed;
}

std::vector<std::complex<float>> ifft(const std::vector<std::complex<float>>& signal) {
    constexpr std::complex<float> im(0, 1);
    auto bitReversed = bitReverse(signal);
    int logN = (int)std::log2(signal.size());
    for (int s = 1; s <= logN; ++s) {
        int m = 1 << s;
        std::complex<float> omega_m = std::exp(2.0f * glm::pi<float>() / m * im);
        for (int k = 0; k < signal.size(); k += m) {
            std::complex<float> omega = 1;
            for (int j = 0; j < m / 2; ++j) {
                std::complex t = omega * bitReversed[k + j + m / 2];
                std::complex u = bitReversed[k + j];
                bitReversed[k + j] = u + t;
                bitReversed[k + j + m / 2] = u - t;
                omega = omega * omega_m;
            }
        }
    }

    const float scale = 1.0f / signal.size();
    for (auto& v : bitReversed) {
        v *= scale;
    }
    return bitReversed;
}

std::vector<std::complex<float>> dft(const std::vector<std::complex<float>>& signal) {
    constexpr std::complex<float> im(0, 1);
    const float N = static_cast<float>(signal.size());
    std::vector<std::complex<float>> result(signal);
    for (int i = 0; i < result.size(); ++i) {
        std::complex<float> Xk(0, 0);
        for (int k = 0; k < signal.size(); ++k) {
            Xk += signal[k] * std::exp(-i * k / N * 2.0f * glm::pi<float>() * im);
        }
        result[i] = Xk;
    }
    return result;
}

std::vector<std::complex<float>> idft(const std::vector<std::complex<float>>& signal) {
    constexpr std::complex<float> im(0, 1);
    const float N = static_cast<float>(signal.size());
    std::vector<std::complex<float>> result(signal);
    for (int i = 0; i < result.size(); ++i) {
        std::complex<float> Xk(0, 0);
        for (int k = 0; k < signal.size(); ++k) {
            Xk += signal[k] * std::exp(i * k / N * 2.0f * glm::pi<float>() * im);
        }
        result[i] = Xk / N;
    }
    return result;
}

TEST(FFT, SimpleTest) {
    std::vector<float> signal(Size);

    std::default_random_engine eng{42};
    std::uniform_real_distribution<float> dist{};
    for (auto& v : signal) {
        v = dist(eng);
    }

    std::vector<int> reversedBits(8);
    for (int i = 0; i < reversedBits.size(); ++i) {
        reversedBits[i] = reverseBits(i, 3);
    }

    auto res = fft(toComplex(signal));

    auto res2 = dft(toComplex(signal));

    std::vector<std::complex<float>> sig2{
        std::complex<float>{1, 0}, std::complex<float>{2, -1}, std::complex<float>{0, -1}, std::complex<float>{-1, +2}};

    auto res3 = dft(sig2);
    auto res4 = fft(sig2);

    auto orig2 = idft(res3);

    auto original = ifft(res);
}

} // namespace crisp