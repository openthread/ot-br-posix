
#include <iostream>
#include "utils/ping.hpp"

#include <CppUTest/TestHarness.h>
#include <CppUTestExt/MockSupport.h>

TEST_GROUP(IcmpPing){};

// use sudo to run this test, or setcap cap_net_raw+ep
TEST(IcmpPing, PingGoogleDNS)
{
    otbr::Utils::IcmpPing ping("8.8.8.8");
    CHECK_TRUE(ping.Send());
    CHECK_TRUE(ping.WaitForResponse(2));
    CHECK_TRUE(ping.Receive());
}
