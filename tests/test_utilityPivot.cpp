#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <thread>

#include "utilityPivot.h"

using namespace systemspn;

TEST(TestUtilityPivot, convertTimestamp) 
{
    long tsMs = UtilityPivot::getCurrentTimestampMs();
    int testTime = 3000;

    for (int i = 0; i <= testTime ; i++ ) {
        long ts1 = tsMs + i ;
        std::pair<long, long> p = UtilityPivot::fromTimestamp(ts1);
        long ts2 = UtilityPivot::toTimestamp(p.first, p.second);

        ASSERT_EQ(ts1, ts2);
    }
}

TEST(TestUtilityPivot, getTimestamp) 
{
    long t1 = UtilityPivot::getCurrentTimestampMs() / 1000L;

    // sleep
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    long t2 = UtilityPivot::getCurrentTimestampMs() / 1000L;

    ASSERT_EQ(t1 + 1, t2);   
}

TEST(TestUtilityPivot, Join)
{
    ASSERT_STREQ(UtilityPivot::join({}).c_str(), "");
    ASSERT_STREQ(UtilityPivot::join({"TEST"}).c_str(), "TEST");
    ASSERT_STREQ(UtilityPivot::join({"TEST", "TOAST", "TASTE"}).c_str(), "TEST, TOAST, TASTE");
    ASSERT_STREQ(UtilityPivot::join({"TEST", "", "TORTOISE"}).c_str(), "TEST, , TORTOISE");
    ASSERT_STREQ(UtilityPivot::join({}, "-").c_str(), "");
    ASSERT_STREQ(UtilityPivot::join({"TEST"}, "-").c_str(), "TEST");
    ASSERT_STREQ(UtilityPivot::join({"TEST", "TOAST", "TASTE"}, "-").c_str(), "TEST-TOAST-TASTE");
    ASSERT_STREQ(UtilityPivot::join({"TEST", "", "TORTOISE"}, "-").c_str(), "TEST--TORTOISE");
    ASSERT_STREQ(UtilityPivot::join({}, "").c_str(), "");
    ASSERT_STREQ(UtilityPivot::join({"TEST"}, "").c_str(), "TEST");
    ASSERT_STREQ(UtilityPivot::join({"TEST", "TOAST", "TASTE"}, "").c_str(), "TESTTOASTTASTE");
    ASSERT_STREQ(UtilityPivot::join({"TEST", "", "TORTOISE"}, "").c_str(), "TESTTORTOISE");
}

TEST(TestUtilityPivot, Split)
{
    ASSERT_EQ(UtilityPivot::split("", '-'), std::vector<std::string>{""});
    ASSERT_EQ(UtilityPivot::split("TEST", '-'), std::vector<std::string>{"TEST"});
    std::vector<std::string> out1{"TEST", "TOAST", "TASTE"};
    ASSERT_EQ(UtilityPivot::split("TEST-TOAST-TASTE", '-'), out1);
    std::vector<std::string> out2{"TEST", "", "TORTOISE"};
    ASSERT_EQ(UtilityPivot::split("TEST--TORTOISE", '-'), out2);
}

TEST(TestUtilityPivot, Logs)
{
    std::string text("This message is at level %s");
    ASSERT_NO_THROW(UtilityPivot::log_debug(text.c_str(), "debug"));
    ASSERT_NO_THROW(UtilityPivot::log_info(text.c_str(), "info"));
    ASSERT_NO_THROW(UtilityPivot::log_warn(text.c_str(), "warning"));
    ASSERT_NO_THROW(UtilityPivot::log_error(text.c_str(), "error"));
    ASSERT_NO_THROW(UtilityPivot::log_fatal(text.c_str(), "fatal"));
}