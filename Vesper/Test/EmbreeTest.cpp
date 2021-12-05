#include "gtest/gtest.h"


#pragma warning(push)
#pragma warning(disable: 4324) // alignment warning
#include <embree3/rtcore.h>
#pragma warning(pop)

TEST(EmbreeTest, Device)
{
    RTCDevice device = rtcNewDevice(nullptr);

    EXPECT_TRUE(device != nullptr);
}