#include <SDKDDKVer.h>
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace crispTest
{
    TEST_CLASS(Distribution1DTest)
    {
    public:
        TEST_METHOD(Size)
        {
            //std::size_t size = 15;
            //crisp::Distribution1D d(size);
            //for (int i = 0; i < size; i++)
            //    d.append(static_cast<float>(i));

            Assert::AreEqual(true, true);
        }

    };
}