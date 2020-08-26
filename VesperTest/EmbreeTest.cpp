#include "gtest/gtest.h"

#include "embree3/rtcore.h"

TEST(EmbreeTest, Device)
{
    RTCDevice device = rtcNewDevice(nullptr);

    EXPECT_TRUE(device != nullptr);
}