#include <SDKDDKVer.h>
#include "CppUnitTest.h"

#include <Vesper/Math/Distribution1D.hpp>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace VesperTest
{
    TEST_CLASS(Distribution1DTest)
    {
    public:
        TEST_METHOD(Size)
        {
            std::size_t size = 15;
            vesper::Distribution1D d(size);
            for (int i = 0; i < size; i++)
                d.append(static_cast<float>(i));

            Assert::AreEqual(size, d.getSize());
        }

    };
}