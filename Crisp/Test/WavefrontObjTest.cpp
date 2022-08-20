#include "gtest/gtest.h"

#include "Crisp/IO/WavefrontObjReader.hpp"

using namespace crisp;

namespace
{
const std::filesystem::path ResourceDir("D:/version-control/Crisp/Resources/Meshes");
}

TEST(WavefrontObjTest, LoadAjax)
{
    const auto mesh = WavefrontObjReader().read(ResourceDir / "ajax.obj");
    EXPECT_EQ(mesh.positions.size(), 409'676);
    EXPECT_EQ(mesh.triangles.size(), 544'566);
}

TEST(WavefrontObjTest, LoadBuddha)
{
    const auto mesh = WavefrontObjReader().read(ResourceDir / "buddha.obj");
    EXPECT_EQ(mesh.positions.size(), 49'990);
    EXPECT_EQ(mesh.triangles.size(), 100'000);
}

TEST(WavefrontObjTest, LoadCube)
{
    const auto mesh = WavefrontObjReader().read(ResourceDir / "cube.obj");
    EXPECT_EQ(mesh.positions.size(), 24);
    EXPECT_EQ(mesh.triangles.size(), 12);
}

TEST(WavefrontObjTest, LoadShaderBall)
{
    const auto mesh = WavefrontObjReader().read(ResourceDir / "shader_ball.obj");
    EXPECT_EQ(mesh.positions.size(), 35'877);
    EXPECT_EQ(mesh.triangles.size(), 67'832);
}

TEST(WavefrontObjTest, LoadCamelHead)
{
    const auto mesh = WavefrontObjReader().read(ResourceDir / "camelhead.obj");
    EXPECT_EQ(mesh.positions.size(), 11'381);
    EXPECT_EQ(mesh.triangles.size(), 22'704);
}

// glm::vec2 compMul(const glm::vec2& z, const glm::vec2& w)
//{
//     return { z[0] * w[0] - z[1] * w[1] , z[0] * w[1] + z[1] * w[0] };
// }
//
// std::vector<glm::vec2> dft(const std::vector<glm::vec2>& signal)
//{
//     uint32_t N = signal.size();
//     std::vector<glm::vec2> freqDom(signal.size());
//     for (uint32_t i = 0; i < N; ++i)
//     {
//         glm::vec2 xk(0.0f);
//         for (uint32_t j = 0; j < N; ++j)
//         {
//             float realB = std::cos(2.0f * glm::pi<float>() * i * j / N);
//             float imagB = -std::sin(2.0f * glm::pi<float>() * i * j / N);
//
//
//             xk += compMul(signal[j], glm::vec2(realB, imagB));
//         }
//
//         freqDom[i] = xk;
//     }
//
//     return freqDom;
// }
//
// std::vector<glm::vec2> fft(const std::vector<glm::vec2>& signal)
//{
//     if (signal.size() == 2)
//         return dft(signal);
//
//     uint32_t N = signal.size();
//
//     std::vector<glm::vec2> even;
//     std::vector<glm::vec2> odd;
//
//     for (uint32_t i = 0; i < N; ++i)
//     {
//         if (i % 2 == 0)
//             even.push_back(signal[i]);
//         else
//             odd.push_back(signal[i]);
//     }
//
//     even = fft(even);
//     odd = fft(odd);
//
//     std::vector<glm::vec2> freqDom(signal.size());
//     for (uint32_t k = 0; k < N / 2; ++k)
//     {
//         float realB = +std::cos(2.0f * glm::pi<float>() * k / N);
//         float imagB = -std::sin(2.0f * glm::pi<float>() * k / N);
//
//         freqDom[k] = even[k] + compMul(odd[k], glm::vec2(realB, imagB));
//         freqDom[k + N / 2] = even[k] - compMul(odd[k], glm::vec2(realB, imagB));
//     }
//
//     return freqDom;
// }
//
// int reverseBits(int k, int N) {
//     int r = 0;
//     while (N) {
//         r = (r << 1) | (k & 1);
//         k >>= 1;
//         N--;
//     }
//
//     return r;
// }
//
// std::vector<glm::vec2> bitReverseCopy(const std::vector<glm::vec2>& a)
//{
//     std::vector<glm::vec2> A(a.size());
//     for (int i = 0; i < a.size(); ++i)
//         A[reverseBits(i, std::log2(a.size()))] = a[i];
//
//     return A;
// }
//
// std::vector<glm::vec2> iterFFT(const std::vector<glm::vec2>& signal)
//{
//     auto A = bitReverseCopy(signal);
//
//     uint32_t N = signal.size();
//     uint32_t logN = std::log2(N);
//
//     for (int s = 1; s <= logN; ++s)
//     {
//         int m = (1 << s);
//
//         for (int k = 0; k < N; k += m)
//         {
//             glm::vec2 w(1, 0);
//
//             for (int j = 0; j < m / 2; ++j)
//             {
//                 w[0] = std::cos(2.0f * glm::pi<float>() / m * j);
//                 w[1] = -std::sin(2.0f * glm::pi<float>() / m * j);
//                 auto t = compMul(w, A[k + j + m / 2]);
//                 auto u = A[k + j];
//                 A[k + j] = u + t;
//                 A[k + j + m / 2] = u - t;
//             }
//         }
//     }
//
//     return A;
// }
//
// std::vector<glm::vec2> iterIFFT(const std::vector<glm::vec2>& signal)
//{
//     auto A = bitReverseCopy(signal);
//
//     uint32_t N = signal.size();
//     uint32_t logN = std::log2(N);
//
//     for (auto& v : A)
//         v /= static_cast<float>(N);
//
//     for (int s = 1; s <= logN; ++s)
//     {
//         int m = (1 << s);
//
//         for (int k = 0; k < N; k += m)
//         {
//             glm::vec2 w(1, 0);
//
//             for (int j = 0; j < m / 2; ++j)
//             {
//                 w[0] = std::cos(2.0f * glm::pi<float>() / m * j);
//                 w[1] = std::sin(2.0f * glm::pi<float>() / m * j);
//                 auto t = compMul(w, A[k + j + m / 2]);
//                 auto u = A[k + j];
//                 A[k + j] = u + t;
//                 A[k + j + m / 2] = u - t;
//             }
//         }
//     }
//
//     return A;
// }
//
// TEST(WavefrontObjTest, FFTTransform)
//{
//     uint32_t N = 256;
//     std::vector<glm::vec2> signal(N);
//
//     for (uint32_t i = 0; i < N; ++i)
//     {
//         float wt = 4.0f * glm::pi<float>()  / static_cast<float>(N) * i;
//         signal[i] = {std::cos(wt), std::sin(wt)};
//     }
//
//     //std::vector<glm::vec2> freqDom = dft(signal);
//     auto freqDom = iterFFT(signal);
//
//     auto invFft = iterIFFT(freqDom);
//
//     /*std::vector<glm::vec2> invFft(N);
//     for (uint32_t i = 0; i < N; ++i)
//     {
//         glm::vec2 xn(0.0f);
//         for (uint32_t j = 0; j < N; ++j)
//         {
//             float realB = std::cos(2.0f * glm::pi<float>() * i * j / N);
//             float imagB = std::sin(2.0f * glm::pi<float>() * i * j / N);
//
//
//             xn += compMul(freqDom[j], glm::vec2(realB, imagB));
//         }
//
//         invFft[i] = xn / static_cast<float>(N);
//     }*/
//
//     for (int i = 0; i < signal.size(); ++i)
//     {
//         //std::cout << "Original:    " << signal[i][0] << " " << signal[i][1] << std::endl;
//         //std::cout << "Inverse FFT: " << invFft[i][0] << " " << invFft[i][1] << std::endl;
//
//         ASSERT_NEAR(signal[i][0], invFft[i][0], 1e-3);
//         ASSERT_NEAR(signal[i][1], invFft[i][1], 1e-3);
//     }
// }
//
// std::vector<std::vector<glm::vec2>> iterFFT2(const std::vector<std::vector<glm::vec2>>& signal)
//{
//     auto temp = signal;
//     for (auto& A : temp)
//     {
//         A = bitReverseCopy(A);
//         uint32_t N = signal.size();
//         uint32_t logN = std::log2(N);
//
//         for (int s = 1; s <= logN; ++s)
//         {
//             int m = (1 << s);
//
//             for (int k = 0; k < N; k += m)
//             {
//                 glm::vec2 w(1, 0);
//
//                 for (int j = 0; j < m / 2; ++j)
//                 {
//                     w[0] = std::cos(2.0f * glm::pi<float>() / m * j);
//                     w[1] = -std::sin(2.0f * glm::pi<float>() / m * j);
//                     auto t = compMul(w, A[k + j + m / 2]);
//                     auto u = A[k + j];
//                     A[k + j] = u + t;
//                     A[k + j + m / 2] = u - t;
//                 }
//             }
//         }
//     }
//
//     // Transpose
//     for (int i = 0; i < temp.size(); ++i)
//         for (int j = i + 1; j < temp.size(); ++j)
//             std::swap(temp[i][j], temp[j][i]);
//
//     // Vertical
//     for (auto& A : temp)
//     {
//         A = bitReverseCopy(A);
//         uint32_t N = signal.size();
//         uint32_t logN = std::log2(N);
//
//         for (int s = 1; s <= logN; ++s)
//         {
//             int m = (1 << s);
//
//             for (int k = 0; k < N; k += m)
//             {
//                 glm::vec2 w(1, 0);
//
//                 for (int j = 0; j < m / 2; ++j)
//                 {
//                     w[0] = std::cos(2.0f * glm::pi<float>() / m * j);
//                     w[1] = -std::sin(2.0f * glm::pi<float>() / m * j);
//                     auto t = compMul(w, A[k + j + m / 2]);
//                     auto u = A[k + j];
//                     A[k + j] = u + t;
//                     A[k + j + m / 2] = u - t;
//                 }
//             }
//         }
//     }
//
//     // Transpose back
//     for (int i = 0; i < temp.size(); ++i)
//         for (int j = i + 1; j < temp.size(); ++j)
//             std::swap(temp[i][j], temp[j][i]);
//
//     return temp;
// }
//
// std::vector<std::vector<glm::vec2>> iterIFFT2(const std::vector<std::vector<glm::vec2>>& signal)
//{
//     auto temp = signal;
//     for (auto& A : temp)
//     {
//         A = bitReverseCopy(A);
//         uint32_t N = signal.size();
//         uint32_t logN = std::log2(N);
//
//         for (auto& v : A)
//             v /= static_cast<float>(N);
//
//         for (int s = 1; s <= logN; ++s)
//         {
//             int m = (1 << s);
//
//             for (int k = 0; k < N; k += m)
//             {
//                 glm::vec2 w(1, 0);
//
//                 for (int j = 0; j < m / 2; ++j)
//                 {
//                     w[0] = std::cos(2.0f * glm::pi<float>() / m * j);
//                     w[1] = std::sin(2.0f * glm::pi<float>() / m * j);
//                     auto t = compMul(w, A[k + j + m / 2]);
//                     auto u = A[k + j];
//                     A[k + j] = u + t;
//                     A[k + j + m / 2] = u - t;
//                 }
//             }
//         }
//     }
//
//     // Transpose
//     for (int i = 0; i < temp.size(); ++i)
//         for (int j = i + 1; j < temp.size(); ++j)
//             std::swap(temp[i][j], temp[j][i]);
//
//     // Vertical
//     for (auto& A : temp)
//     {
//         A = bitReverseCopy(A);
//         uint32_t N = signal.size();
//         uint32_t logN = std::log2(N);
//
//         for (auto& v : A)
//             v /= static_cast<float>(N);
//
//         for (int s = 1; s <= logN; ++s)
//         {
//             int m = (1 << s);
//
//             for (int k = 0; k < N; k += m)
//             {
//                 glm::vec2 w(1, 0);
//
//                 for (int j = 0; j < m / 2; ++j)
//                 {
//                     w[0] = std::cos(2.0f * glm::pi<float>() / m * j);
//                     w[1] = -std::sin(2.0f * glm::pi<float>() / m * j);
//                     auto t = compMul(w, A[k + j + m / 2]);
//                     auto u = A[k + j];
//                     A[k + j] = u + t;
//                     A[k + j + m / 2] = u - t;
//                 }
//             }
//         }
//     }
//
//     // Transpose back
//     for (int i = 0; i < temp.size(); ++i)
//         for (int j = i + 1; j < temp.size(); ++j)
//             std::swap(temp[i][j], temp[j][i]);
//
//     return temp;
// }
//
// TEST(WavefrontObjTest, FFTTransform2D)
//{
//     uint32_t N = 256;
//     std::vector<std::vector<glm::vec2>> signal(N, std::vector<glm::vec2>(N));
//
//     for (uint32_t i = 0; i < N; ++i)
//     {
//         for (uint32_t j = 0; j < N; ++j)
//         {
//             float wt = 2.0f * glm::pi<float>() / static_cast<float>(N) * i;
//             float wz = 2.0f * glm::pi<float>() / static_cast<float>(N) * j;
//             signal[i][j] = { std::cos(wt) * std::cos(wz), std::sin(wt) * std::sin(wz) };
//         }
//
//     }
//
//     //std::vector<glm::vec2> freqDom = dft(signal);
//     auto freqDom = iterFFT2(signal);
//
//     auto invFft = iterIFFT2(freqDom);
//
//     /*std::vector<glm::vec2> invFft(N);
//     for (uint32_t i = 0; i < N; ++i)
//     {
//         glm::vec2 xn(0.0f);
//         for (uint32_t j = 0; j < N; ++j)
//         {
//             float realB = std::cos(2.0f * glm::pi<float>() * i * j / N);
//             float imagB = std::sin(2.0f * glm::pi<float>() * i * j / N);
//
//
//             xn += compMul(freqDom[j], glm::vec2(realB, imagB));
//         }
//
//         invFft[i] = xn / static_cast<float>(N);
//     }*/
//
//     for (int i = 0; i < signal.size(); ++i)
//     {
//         for (int j = 0; j < signal[i].size(); ++j)
//         {
//             //std::cout << "Original:    " << signal[i][0] << " " << signal[i][1] << std::endl;
//              //std::cout << "Inverse FFT: " << invFft[i][0] << " " << invFft[i][1] << std::endl;
//
//             ASSERT_NEAR(signal[i][j][0], invFft[i][j][0], 1e-3);
//             ASSERT_NEAR(signal[i][j][1], invFft[i][j][1], 1e-3);
//         }
//     }
// }
//
// TEST(WavefrontObjTest, BitReversal)
//{
//     uint32_t N = 8;
//     //ASSERT_EQ(reverseBits(0, N))
// }