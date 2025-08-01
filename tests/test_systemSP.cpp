#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <plugin_api.h>
#include <queue>

#include "notifySystemSp.h"
#include "constantsSystem.h"
#include "utilityPivot.h"

using namespace DatapointUtility;
using namespace systemspn;


static std::string configure = QUOTE({
    "enable" :{
        "value": "true"
    },
    "exchanged_data": {
        "value" : {
            "exchanged_data": {
                "datapoints" : [
                    {
                        "label":"TS-1",
                        "pivot_id":"M_2367_3_15_4",
                        "pivot_type":"SpsTyp",
                        "pivot_subtypes": [
                            "acces"
                        ],
                        "ts_syst_cycle": 30,
                        "protocols":[
                            {
                                "name":"IEC104",
                                "typeid":"M_ME_NC_1",
                                "address":"3271612"
                            }
                        ]
                    },
                    {
                        "label":"TS-2",
                        "pivot_id":"M_2367_3_15_5",
                        "pivot_type":"DpsTyp",
                        "pivot_subtypes": [
                            "acces"
                        ],
                        "ts_syst_cycle": 30,
                        "protocols":[
                            {
                                "name":"IEC104",
                                "typeid":"M_ME_NC_2",
                                "address":"3271613"
                            }
                        ]
                    },
                    {
                        "label":"TS-3",
                        "pivot_id":"M_2367_3_15_6",
                        "pivot_type":"SpsTyp",
                        "pivot_subtypes": [
                            "prt.inf"
                        ],
                        "protocols":[
                            {
                                "name":"IEC104",
                                "typeid":"M_ME_NC_3",
                                "address":"3271614"
                            }
                        ]
                    },
                    {
                        "label":"TS-4",
                        "pivot_id":"M_2367_3_15_7",
                        "pivot_type":"DpsTyp",
                        "pivot_subtypes": [
                            "prt.inf"
                        ],
                        "protocols":[
                            {
                                "name":"IEC104",
                                "typeid":"M_ME_NC_4",
                                "address":"3271615"
                            }
                        ]
                    }
                ]
            }
        }
    }
});

std::string emptyConfig = QUOTE({
    "enable" :{
        "value": "true"
    },
    "exchanged_data": {
        "value" : {
            "exchanged_data": {
                "datapoints" : []
            }
        }
    }
});

extern "C" {
	PLUGIN_INFORMATION *plugin_info();
	PLUGIN_HANDLE plugin_init(ConfigCategory *config);
    void plugin_reconfigure(PLUGIN_HANDLE *handle, const std::string& newConfig);
    bool plugin_deliver(PLUGIN_HANDLE handle,
                    const std::string& deliveryName,
                    const std::string& notificationName,
                    const std::string& triggerReason,
                    const std::string& message);
    void plugin_registerIngest(PLUGIN_HANDLE *handle, void *func, void *data);
    void plugin_shutdown(PLUGIN_HANDLE *handle);
};

class TestSystemSp : public testing::Test
{
protected:
    NotifySystemSp *filter = nullptr;  // Object on which we call for tests

    // Setup is ran for every tests, so each variable are reinitialised
    void SetUp() override
    {
        PLUGIN_INFORMATION *info = plugin_info();
		ConfigCategory *config =  new ConfigCategory("systemsp", info->config);
		ASSERT_NE(config, nullptr);
		config->setItemsValueFromDefault();
		config->setValue("enable", "true");

		PLUGIN_HANDLE handle = nullptr;
        ASSERT_NO_THROW(handle = plugin_init(config));
		filter = static_cast<NotifySystemSp *>(handle);

        ASSERT_NO_THROW(plugin_registerIngest(reinterpret_cast<PLUGIN_HANDLE*>(filter), (void*)ingestCallback, nullptr));

        ASSERT_NO_THROW(plugin_reconfigure(reinterpret_cast<PLUGIN_HANDLE*>(filter), configure));
        ASSERT_TRUE(filter->isEnabled());

        // Wait 100 ms to make sure the cyclic TS from the initialization of the plugin do not interfere with the tests
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        resetCounters();
        clearReadings();
    }

    // TearDown is ran for every tests, so each variable are destroyed again
    void TearDown() override
    {
        if (filter) {
            ASSERT_NO_THROW(plugin_shutdown(reinterpret_cast<PLUGIN_HANDLE*>(filter)));
        }
        std::lock_guard<std::recursive_mutex> guard(storedReadingsMutex);
        storedReadings = {};
    }

    static void resetCounters() {
        std::lock_guard<std::recursive_mutex> guard(storedReadingsMutex);
        ingestCallbackCalled = 0;
    }

    template<class... Args>
    static void debug_print(std::string format, Args&&... args) {
        printf(format.append("\n").c_str(), std::forward<Args>(args)...);
        fflush(stdout);
    }

    static void ingestCallback(void *data, void *readingPtr) {
        Reading* reading = static_cast<Reading*>(readingPtr);
        if (reading == nullptr) {
            debug_print("ingestCallback : Error: received null reading");
            return;
        }
        debug_print("ingestCallback called -> asset: (%s)", reading->getAssetName().c_str());

        std::vector<Datapoint*> dataPoints = reading->getReadingData();

        debug_print("  number of readings: %lu", dataPoints.size());

        // for (Datapoint* sdp : dataPoints) {
        //     debug_print("name: %s value: %s", sdp->getName().c_str(),
        //     sdp->getData().toString().c_str());
        // }
        std::lock_guard<std::recursive_mutex> guard(storedReadingsMutex);
        storedReadings.push(std::make_shared<Reading>(*reading));

        ingestCallbackCalled++;
    }

    static std::shared_ptr<Reading> popFrontReading() {
        std::lock_guard<std::recursive_mutex> guard(storedReadingsMutex);
        std::shared_ptr<Reading> currentReading = nullptr;
        if (!storedReadings.empty()) {
            currentReading = storedReadings.front();
            storedReadings.pop();
        }
        return currentReading;
    }

    static void clearReadings() {
        std::lock_guard<std::recursive_mutex> guard(storedReadingsMutex);
        storedReadings = {};
    }

    static void waitUntil(int& counter, int expectedCount, int timeoutMs) {
        int waitTimeMs = 100;
        int attempts = timeoutMs / waitTimeMs;
        bool expectedCountNotReached = true;
        {
            // Counter is often one of the readings counter, so lock-guard it
            std::lock_guard<std::recursive_mutex> guard(storedReadingsMutex);
            expectedCountNotReached = counter < expectedCount;
        }
        while (expectedCountNotReached && (attempts > 0)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(waitTimeMs));
            attempts--;
            {
                // Counter is often one of the readings counter, so lock-guard it
                std::lock_guard<std::recursive_mutex> guard(storedReadingsMutex);
                expectedCountNotReached = counter < expectedCount;
            }
        }
    }

    static bool hasChild(Datapoint& dp, const std::string& childLabel) {
        DatapointValue& dpv = dp.getData();

        auto dps = dpv.getDpVec();

        for (auto sdp : *dps) {
            if (sdp->getName() == childLabel) {
                return true;
            }
        }

        return false;
    }

    static Datapoint* getChild(Datapoint& dp, const std::string& childLabel) {
        DatapointValue& dpv = dp.getData();

        auto dps = dpv.getDpVec();

        for (Datapoint* childDp : *dps) {
            if (childDp->getName() == childLabel) {
                return childDp;
            }
        }

        return nullptr;
    }

    template <typename T>
    static T callOnLastPathElement(Datapoint& dp, const std::string& childPath, std::function<T(Datapoint&, const std::string&)> func) {
        if (childPath.find(".") != std::string::npos) {
            // Check if the first element in the path is a child of current datapoint
            std::vector<std::string> splittedPath = UtilityPivot::split(childPath, '.');
            const std::string& topNode(splittedPath[0]);
            Datapoint* child = getChild(dp, topNode);
            if (child == nullptr) {
                return static_cast<T>(0);
            }
            // If it is, call recursively this function on the datapoint found with the rest of the path
            splittedPath.erase(splittedPath.begin());
            const std::string& remainingPath(UtilityPivot::join(splittedPath, "."));
            return callOnLastPathElement(*child, remainingPath, func);
        }
        else {
            // If last element of the path reached, call function on it
            return func(dp, childPath);
        }
    }

    static int64_t getIntValue(const Datapoint& dp) {
        return dp.getData().toInt();
    }

    static std::string getStrValue(const Datapoint& dp) {
        return dp.getData().toStringValue();
    }

    static bool hasObject(const Reading& reading, const std::string& label) {
        std::vector<Datapoint*> dataPoints = reading.getReadingData();

        for (Datapoint* dp : dataPoints) {
            if (dp->getName() == label) {
                return true;
            }
        }

        return false;
    }

    static Datapoint* getObject(const Reading& reading, const std::string& label) {
        std::vector<Datapoint*> dataPoints = reading.getReadingData();

        for (Datapoint* dp : dataPoints) {
            if (dp->getName() == label) {
                return dp;
            }
        }

        return nullptr;
    }

    struct ReadingInfo {
        std::string type;
        std::string value;
    };
    static void validateReading(std::shared_ptr<Reading> currentReading, const std::string& assetName, const std::string& rootObjectName,
                                const std::vector<std::string>& allAttributeNames, const std::map<std::string, ReadingInfo>& attributes) {
        ASSERT_NE(nullptr, currentReading.get()) << assetName << ": Invalid reading";
        ASSERT_EQ(assetName, currentReading->getAssetName());
        // Validate data_object structure received
        ASSERT_TRUE(hasObject(*currentReading, rootObjectName)) << assetName << ": " << rootObjectName << " not found";
        Datapoint* data_object = getObject(*currentReading, rootObjectName);
        ASSERT_NE(nullptr, data_object) << assetName << ": " << rootObjectName << " is null";
        // Validate existance of the required keys and non-existance of the others
        for (const auto &kvp : attributes) {
            const std::string& name(kvp.first);
            ASSERT_TRUE(std::find(allAttributeNames.begin(), allAttributeNames.end(), name) != allAttributeNames.end())
                << assetName << ": Attribute not listed in full list: " << name;
        }
        for (const std::string& name : allAttributeNames) {
            bool attributeIsExpected = static_cast<bool>(attributes.count(name));
            std::function<bool(Datapoint&, const std::string&)> hasChildFn(&hasChild);
            ASSERT_EQ(callOnLastPathElement(*data_object, name, hasChildFn),
                attributeIsExpected) << assetName << ": Attribute " << (attributeIsExpected ? "not found: " : "should not exist: ") << name;
        }
        // Validate value and type of each key
        for (auto const& kvp : attributes) {
            const std::string& name = kvp.first;
            const std::string& type = kvp.second.type;
            const std::string& expectedValue = kvp.second.value;
            std::function<Datapoint*(Datapoint&, const std::string&)> getChildFn(&getChild);
            if (type == std::string("string")) {
                ASSERT_EQ(expectedValue, getStrValue(*callOnLastPathElement(*data_object, name, getChildFn))) << assetName << ": Unexpected value for attribute " << name;
            }
            else if (type == std::string("int64_t")) {
                ASSERT_EQ(std::stoll(expectedValue), getIntValue(*callOnLastPathElement(*data_object, name, getChildFn))) << assetName << ": Unexpected value for attribute " << name;
            }
            else if (type == std::string("int64_t_range")) {
                auto splitted = UtilityPivot::split(expectedValue, ';');
                ASSERT_EQ(splitted.size(), 2);
                const std::string& expectedRangeMin = splitted.front();
                const std::string& expectedRangeMax = splitted.back();
                int64_t value = getIntValue(*callOnLastPathElement(*data_object, name, getChildFn));
                ASSERT_GE(value, std::stoll(expectedRangeMin)) << assetName << ": Value lower than min for attribute " << name;
                ASSERT_LE(value, std::stoll(expectedRangeMax)) << assetName << ": Value higher than max for attribute " << name;
            }
            else {
                FAIL() << assetName << ": Unknown type: " << type;
            }
        }
    }

    static int ingestCallbackCalled;
    static std::queue<std::shared_ptr<Reading>> storedReadings;
    static std::recursive_mutex storedReadingsMutex;
    static const std::vector<std::string> allPivotAttributeNames;
};

int TestSystemSp::ingestCallbackCalled = 0;
std::queue<std::shared_ptr<Reading>> TestSystemSp::storedReadings;
std::recursive_mutex TestSystemSp::storedReadingsMutex;
const std::vector<std::string> TestSystemSp::allPivotAttributeNames = {
    // TS messages
    "GTIS.ComingFrom", "GTIS.Identifier", "GTIS.Cause.stVal", "GTIS.TmValidity.stVal", "GTIS.TmOrg.stVal",
    "GTIS.SpsTyp.stVal", "GTIS.SpsTyp.q.Validity", "GTIS.SpsTyp.q.Source", "GTIS.SpsTyp.q.DetailQuality.oldData",
    "GTIS.SpsTyp.t.SecondSinceEpoch", "GTIS.SpsTyp.t.FractionOfSecond", "GTIS.SpsTyp.t.TimeQuality.clockNotSynchronized",
    "GTIS.DpsTyp.stVal", "GTIS.DpsTyp.q.Validity", "GTIS.DpsTyp.q.Source", "GTIS.DpsTyp.q.DetailQuality.oldData",
    "GTIS.DpsTyp.t.SecondSinceEpoch", "GTIS.DpsTyp.t.FractionOfSecond", "GTIS.DpsTyp.t.TimeQuality.clockNotSynchronized",
    // TM messages
    "GTIM.ComingFrom", "GTIM.Identifier", "GTIM.Cause.stVal", "GTIM.TmValidity.stVal", "GTIM.TmOrg.stVal",
    "GTIM.MvTyp.mag.i", "GTIM.MvTyp.q.Validity", "GTIS.MvTyp.q.Source", "GTIM.MvTyp.q.DetailQuality.oldData",
    "GTIM.MvTyp.t.SecondSinceEpoch", "GTIM.MvTyp.t.FractionOfSecond", "GTIM.MvTyp.t.TimeQuality.clockNotSynchronized",
    // TC/TVC messages
    "GTIC.ComingFrom", "GTIC.Identifier", "GTIC.Cause.stVal", "GTIC.TmValidity.stVal", "GTIC.TmOrg.stVal",
    "GTIC.SpcTyp.stVal", "GTIC.SpcTyp.ctlVal", "GTIC.SpcTyp.q.Validity", "GTIS.SpcTyp.q.Source", "GTIC.SpcTyp.q.DetailQuality.oldData",
    "GTIC.SpcTyp.t.SecondSinceEpoch", "GTIC.SpcTyp.t.FractionOfSecond", "GTIC.SpcTyp.t.TimeQuality.clockNotSynchronized",
    "GTIC.DpcTyp.stVal", "GTIC.DpcTyp.ctlVal", "GTIC.DpcTyp.q.Validity", "GTIS.DpcTyp.q.Source", "GTIC.DpcTyp.q.DetailQuality.oldData",
    "GTIC.DpcTyp.t.SecondSinceEpoch", "GTIC.DpcTyp.t.FractionOfSecond", "GTIC.DpcTyp.t.TimeQuality.clockNotSynchronized",
    "GTIC.IncTyp.stVal", "GTIC.IncTyp.ctlVal", "GTIC.IncTyp.q.Validity", "GTIS.IncTyp.q.Source", "GTIC.IncTyp.q.DetailQuality.oldData",
    "GTIC.IncTyp.t.SecondSinceEpoch", "GTIC.IncTyp.t.FractionOfSecond", "GTIC.IncTyp.t.TimeQuality.clockNotSynchronized",
};

TEST_F(TestSystemSp, CyclicAccessMessages)
{
	std::string customConfig = QUOTE({
        "enable" :{
            "value": "true"
        },
        "exchanged_data": {
            "value" : {
                "exchanged_data": {
                    "datapoints" : [
                        {
                            "label":"TS-1",
                            "pivot_id":"M_2367_3_15_4",
                            "pivot_type":"SpsTyp",
                            "pivot_subtypes": [
                                "acces"
                            ],
                            "ts_syst_cycle": 2,
                            "protocols":[
                                {
                                    "name":"IEC104",
                                    "typeid":"M_ME_NC_1",
                                    "address":"3271612"
                                }
                            ]
                        },
                        {
                            "label":"TS-2",
                            "pivot_id":"M_2367_3_15_5",
                            "pivot_type":"DpsTyp",
                            "pivot_subtypes": [
                                "acces"
                            ],
                            "ts_syst_cycle": 3,
                            "protocols":[
                                {
                                    "name":"IEC104",
                                    "typeid":"M_ME_NC_2",
                                    "address":"3271613"
                                }
                            ]
                        }
                    ]
                }
            }
        }
    });

    debug_print("Reconfigure plugin");
    ASSERT_NO_THROW(plugin_reconfigure(reinterpret_cast<PLUGIN_HANDLE*>(filter), customConfig));
    ASSERT_TRUE(filter->isEnabled());
    // All messages are sent immediately at startup (but sent from threads so still use wait)
    waitUntil(ingestCallbackCalled, 2, 100);
    ASSERT_EQ(ingestCallbackCalled, 2);
    resetCounters();
    // Readings are received in unknown order as they are sent at the same time but from different threads
    std::shared_ptr<Reading> currentReading;
    auto pivotTimestampPair = UtilityPivot::fromTimestamp(UtilityPivot::getCurrentTimestampMs());
    long sec = pivotTimestampPair.first;
    for(int i=0 ; i<2 ; i++) {
        currentReading = popFrontReading();
        ASSERT_NE(nullptr, currentReading.get()) << "Invalid reading at loop " << i;
        if (currentReading->getAssetName() == "TS-1") {
            validateReading(currentReading, "TS-1", "PIVOT", allPivotAttributeNames, {
                {"GTIS.Identifier", {"string", "M_2367_3_15_4"}},
                {"GTIS.Cause.stVal", {"int64_t", "3"}},
                {"GTIS.TmOrg.stVal", {"string", "substituted"}},
                {"GTIS.SpsTyp.stVal", {"int64_t", "1"}},
                {"GTIS.SpsTyp.t.SecondSinceEpoch", {"int64_t_range", std::to_string(sec-1) + ";" + std::to_string(sec)}},
                {"GTIS.SpsTyp.t.FractionOfSecond", {"int64_t_range", "0;99999999"}},
                {"GTIS.SpsTyp.q.Source", {"string", "substituted"}},
            });
        }
        else {
            validateReading(currentReading, "TS-2", "PIVOT", allPivotAttributeNames, {
                {"GTIS.Identifier", {"string", "M_2367_3_15_5"}},
                {"GTIS.Cause.stVal", {"int64_t", "3"}},
                {"GTIS.TmOrg.stVal", {"string", "substituted"}},
                {"GTIS.DpsTyp.stVal", {"string", "on"}},
                {"GTIS.DpsTyp.t.SecondSinceEpoch", {"int64_t_range", std::to_string(sec-1) + ";" + std::to_string(sec)}},
                {"GTIS.DpsTyp.t.FractionOfSecond", {"int64_t_range", "0;99999999"}},
                {"GTIS.DpsTyp.q.Source", {"string", "substituted"}},
            });
        }
        if(HasFatalFailure()) return;
    }

    // Nothing received 1 second after startup
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ASSERT_EQ(ingestCallbackCalled, 0);

    // First timer expires 2 seconds after startup
    debug_print("Wait for TS-1 first cyclic call...");
    waitUntil(ingestCallbackCalled, 1, 1100);
    ASSERT_EQ(ingestCallbackCalled, 1);
    resetCounters();

    pivotTimestampPair = UtilityPivot::fromTimestamp(UtilityPivot::getCurrentTimestampMs());
    sec = pivotTimestampPair.first;
    currentReading = popFrontReading();
    validateReading(currentReading, "TS-1", "PIVOT", allPivotAttributeNames, {
        {"GTIS.Identifier", {"string", "M_2367_3_15_4"}},
        {"GTIS.Cause.stVal", {"int64_t", "3"}},
        {"GTIS.TmOrg.stVal", {"string", "substituted"}},
        {"GTIS.SpsTyp.stVal", {"int64_t", "1"}},
        {"GTIS.SpsTyp.t.SecondSinceEpoch", {"int64_t_range", std::to_string(sec-1) + ";" + std::to_string(sec)}},
        {"GTIS.SpsTyp.t.FractionOfSecond", {"int64_t_range", "0;99999999"}},
        {"GTIS.SpsTyp.q.Source", {"string", "substituted"}},
    });
    if(HasFatalFailure()) return;

    // Second timer expires 3 seconds after startup
    debug_print("Wait for TS-2 first cyclic call...");
    waitUntil(ingestCallbackCalled, 1, 1100);
    ASSERT_EQ(ingestCallbackCalled, 1);
    resetCounters();

    pivotTimestampPair = UtilityPivot::fromTimestamp(UtilityPivot::getCurrentTimestampMs());
    sec = pivotTimestampPair.first;
    currentReading = popFrontReading();
    validateReading(currentReading, "TS-2", "PIVOT", allPivotAttributeNames, {
        {"GTIS.Identifier", {"string", "M_2367_3_15_5"}},
        {"GTIS.Cause.stVal", {"int64_t", "3"}},
        {"GTIS.TmOrg.stVal", {"string", "substituted"}},
        {"GTIS.DpsTyp.stVal", {"string", "on"}},
        {"GTIS.DpsTyp.t.SecondSinceEpoch", {"int64_t_range", std::to_string(sec-1) + ";" + std::to_string(sec)}},
        {"GTIS.DpsTyp.t.FractionOfSecond", {"int64_t_range", "0;99999999"}},
        {"GTIS.DpsTyp.q.Source", {"string", "substituted"}},
    });
    if(HasFatalFailure()) return;

    // First timer expires again 4 seconds after startup
    debug_print("Wait for TS-1 second cyclic call...");
    waitUntil(ingestCallbackCalled, 1, 1100);
    ASSERT_EQ(ingestCallbackCalled, 1);
    resetCounters();

    pivotTimestampPair = UtilityPivot::fromTimestamp(UtilityPivot::getCurrentTimestampMs());
    sec = pivotTimestampPair.first;
    currentReading = popFrontReading();
    validateReading(currentReading, "TS-1", "PIVOT", allPivotAttributeNames, {
        {"GTIS.Identifier", {"string", "M_2367_3_15_4"}},
        {"GTIS.Cause.stVal", {"int64_t", "3"}},
        {"GTIS.TmOrg.stVal", {"string", "substituted"}},
        {"GTIS.SpsTyp.stVal", {"int64_t", "1"}},
        {"GTIS.SpsTyp.t.SecondSinceEpoch", {"int64_t_range", std::to_string(sec-1) + ";" + std::to_string(sec)}},
        {"GTIS.SpsTyp.t.FractionOfSecond", {"int64_t_range", "0;99999999"}},
        {"GTIS.SpsTyp.q.Source", {"string", "substituted"}},
    });
    if(HasFatalFailure()) return;

    // Nothing received 5 second after startup
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ASSERT_EQ(ingestCallbackCalled, 0);

    // First and second timers expires together 6 seconds after startup
    debug_print("Wait for TS-1 third and TS-2 second cyclic call...");
    waitUntil(ingestCallbackCalled, 2, 1100);
    ASSERT_EQ(ingestCallbackCalled, 2);
    resetCounters();

    // Readings are received in unknown order as they are sent at the same time but from different threads
    pivotTimestampPair = UtilityPivot::fromTimestamp(UtilityPivot::getCurrentTimestampMs());
    sec = pivotTimestampPair.first;
    for(int i=0 ; i<2 ; i++) {
        currentReading = popFrontReading();
        ASSERT_NE(nullptr, currentReading.get()) << "Invalid reading at loop " << i;
        if (currentReading->getAssetName() == "TS-1") {
            validateReading(currentReading, "TS-1", "PIVOT", allPivotAttributeNames, {
                {"GTIS.Identifier", {"string", "M_2367_3_15_4"}},
                {"GTIS.Cause.stVal", {"int64_t", "3"}},
                {"GTIS.TmOrg.stVal", {"string", "substituted"}},
                {"GTIS.SpsTyp.stVal", {"int64_t", "1"}},
                {"GTIS.SpsTyp.t.SecondSinceEpoch", {"int64_t_range", std::to_string(sec-1) + ";" + std::to_string(sec)}},
                {"GTIS.SpsTyp.t.FractionOfSecond", {"int64_t_range", "0;99999999"}},
                {"GTIS.SpsTyp.q.Source", {"string", "substituted"}},
            });
        }
        else {
            validateReading(currentReading, "TS-2", "PIVOT", allPivotAttributeNames, {
                {"GTIS.Identifier", {"string", "M_2367_3_15_5"}},
                {"GTIS.Cause.stVal", {"int64_t", "3"}},
                {"GTIS.TmOrg.stVal", {"string", "substituted"}},
                {"GTIS.DpsTyp.stVal", {"string", "on"}},
                {"GTIS.DpsTyp.t.SecondSinceEpoch", {"int64_t_range", std::to_string(sec-1) + ";" + std::to_string(sec)}},
                {"GTIS.DpsTyp.t.FractionOfSecond", {"int64_t_range", "0;99999999"}},
                {"GTIS.DpsTyp.q.Source", {"string", "substituted"}},
            });
        }
        if(HasFatalFailure()) return;
    }

    debug_print("Load empty config");
    ASSERT_NO_THROW(plugin_reconfigure(reinterpret_cast<PLUGIN_HANDLE*>(filter), emptyConfig));
    ASSERT_TRUE(filter->isEnabled());
    // Validate that no message is sent any more after a new config was loaded
    std::this_thread::sleep_for(std::chrono::milliseconds(4000));
    ASSERT_EQ(ingestCallbackCalled, 0);
}

TEST_F(TestSystemSp, ConnectionLossMessages)
{
	std::string customConfig = QUOTE({
        "enable" :{
            "value": "true"
        },
        "exchanged_data": {
            "value" : {
                "exchanged_data": {
                    "datapoints" : [
                         {
                            "label":"TS-3",
                            "pivot_id":"M_2367_3_15_6",
                            "pivot_type":"SpsTyp",
                            "pivot_subtypes": [
                                "prt.inf"
                            ],
                            "protocols":[
                                {
                                    "name":"IEC104",
                                    "typeid":"M_ME_NC_3",
                                    "address":"3271614"
                                }
                            ]
                        },
                        {
                            "label":"TS-4",
                            "pivot_id":"M_2367_3_15_7",
                            "pivot_type":"DpsTyp",
                            "pivot_subtypes": [
                                "prt.inf"
                            ],
                            "protocols":[
                                {
                                    "name":"IEC104",
                                    "typeid":"M_ME_NC_4",
                                    "address":"3271615"
                                }
                            ]
                        }
                    ]
                }
            }
        }
    });

    debug_print("Reconfigure plugin");
    ASSERT_NO_THROW(plugin_reconfigure(reinterpret_cast<PLUGIN_HANDLE*>(filter), customConfig));
    ASSERT_TRUE(filter->isEnabled());

    debug_print("Testing invalid notifications...");
    // Send notification with invalid json
    std::string notifInvalidJson = QUOTE({42});
    ASSERT_FALSE(plugin_deliver(reinterpret_cast<PLUGIN_HANDLE*>(filter), "dummyDeliveryName", "dummyNotificationName",
                 notifInvalidJson, "dummyMessage"));
    ASSERT_EQ(ingestCallbackCalled, 0);

    // Send notification with missing "asset"
    std::string notifMissingAsset = QUOTE({
        "reason": "connected"
    });
    ASSERT_FALSE(plugin_deliver(reinterpret_cast<PLUGIN_HANDLE*>(filter), "dummyDeliveryName", "dummyNotificationName",
                 notifMissingAsset, "dummyMessage"));
    ASSERT_EQ(ingestCallbackCalled, 0);

    // Send notification with bad "asset" type
    std::string notifBadAsset = QUOTE({
        "asset": 42,
        "reason": "connected"
    });
    ASSERT_FALSE(plugin_deliver(reinterpret_cast<PLUGIN_HANDLE*>(filter), "dummyDeliveryName", "dummyNotificationName",
                 notifBadAsset, "dummyMessage"));
    ASSERT_EQ(ingestCallbackCalled, 0);

    // Send notification with unknown "asset" value
    std::string notifUnknownAsset = QUOTE({
        "asset": "test",
        "reason": "connected"
    });
    ASSERT_FALSE(plugin_deliver(reinterpret_cast<PLUGIN_HANDLE*>(filter), "dummyDeliveryName", "dummyNotificationName",
                 notifUnknownAsset, "dummyMessage"));
    ASSERT_EQ(ingestCallbackCalled, 0);

    // Send notification with missing "reason"
    std::string notifMissingReason = QUOTE({
        "asset": "prt.inf"
    });
    ASSERT_FALSE(plugin_deliver(reinterpret_cast<PLUGIN_HANDLE*>(filter), "dummyDeliveryName", "dummyNotificationName",
                 notifMissingReason, "dummyMessage"));
    ASSERT_EQ(ingestCallbackCalled, 0);

    // Send notification with bad "reason" type
    std::string notifBadReason = QUOTE({
        "asset": "prt.inf",
        "reason": 42
    });
    ASSERT_FALSE(plugin_deliver(reinterpret_cast<PLUGIN_HANDLE*>(filter), "dummyDeliveryName", "dummyNotificationName",
                 notifBadReason, "dummyMessage"));
    ASSERT_EQ(ingestCallbackCalled, 0);

    // Send notification with unknown "reason" value
    std::string notifUnknownReason = QUOTE({
        "asset": "prt.inf",
        "reason": "test"
    });
    ASSERT_FALSE(plugin_deliver(reinterpret_cast<PLUGIN_HANDLE*>(filter), "dummyDeliveryName", "dummyNotificationName",
                 notifUnknownReason, "dummyMessage"));
    ASSERT_EQ(ingestCallbackCalled, 0);

    debug_print("Testing connected notification");
    // Send notification with gi_status finished
    std::string notifConnected = QUOTE({
        "asset": "gi_status",
        "reason": "finished"
    });
    ASSERT_TRUE(plugin_deliver(reinterpret_cast<PLUGIN_HANDLE*>(filter), "dummyDeliveryName", "dummyNotificationName",
                notifConnected, "dummyMessage"));
    ASSERT_EQ(ingestCallbackCalled, 4);
    resetCounters();

    // Readings are received in unknown order as it depends on json parsing order
    std::shared_ptr<Reading> currentReading;
    auto pivotTimestampPair = UtilityPivot::fromTimestamp(UtilityPivot::getCurrentTimestampMs());
    long sec = pivotTimestampPair.first;
    for(int i=0 ; i<2 ; i++) {
        currentReading = popFrontReading();
        ASSERT_NE(nullptr, currentReading.get()) << "Invalid reading at loop " << i;
        if (currentReading->getAssetName() == "TS-3") {
            validateReading(currentReading, "TS-3", "PIVOT", allPivotAttributeNames, {
                {"GTIS.Identifier", {"string", "M_2367_3_15_6"}},
                {"GTIS.Cause.stVal", {"int64_t", "3"}},
                {"GTIS.TmOrg.stVal", {"string", "substituted"}},
                {"GTIS.SpsTyp.stVal", {"int64_t", "1"}},
                {"GTIS.SpsTyp.t.SecondSinceEpoch", {"int64_t_range", std::to_string(sec-1) + ";" + std::to_string(sec)}},
                {"GTIS.SpsTyp.t.FractionOfSecond", {"int64_t_range", "0;99999999"}},
                {"GTIS.SpsTyp.q.Source", {"string", "substituted"}},
            });
        }
        else {
            validateReading(currentReading, "TS-4", "PIVOT", allPivotAttributeNames, {
                {"GTIS.Identifier", {"string", "M_2367_3_15_7"}},
                {"GTIS.Cause.stVal", {"int64_t", "3"}},
                {"GTIS.TmOrg.stVal", {"string", "substituted"}},
                {"GTIS.DpsTyp.stVal", {"string", "on"}},
                {"GTIS.DpsTyp.t.SecondSinceEpoch", {"int64_t_range", std::to_string(sec-1) + ";" + std::to_string(sec)}},
                {"GTIS.DpsTyp.t.FractionOfSecond", {"int64_t_range", "0;99999999"}},
                {"GTIS.DpsTyp.q.Source", {"string", "substituted"}},
            });
        }
        if(HasFatalFailure()) return;
    }

    debug_print("Testing connection connx_status not connected notification");
    // Send notification with reason "connection lost"
    std::string notifConnectionLost = QUOTE({
        "asset": "connx_status",
        "reason": "not connected"
    });
    ASSERT_FALSE(plugin_deliver(reinterpret_cast<PLUGIN_HANDLE*>(filter), "dummyDeliveryName", "dummyNotificationName",
                notifConnectionLost, "dummyMessage"));
    //We ignore the connx
    ASSERT_EQ(ingestCallbackCalled, 0);
    resetCounters();

    // Readings are received in unknown order as it depends on json parsing order
    pivotTimestampPair = UtilityPivot::fromTimestamp(UtilityPivot::getCurrentTimestampMs());
    sec = pivotTimestampPair.first;
    for(int i=0 ; i<2 ; i++) {
        currentReading = popFrontReading();
        ASSERT_NE(nullptr, currentReading.get()) << "Invalid reading at loop " << i;
        if (currentReading->getAssetName() == "TS-3") {
            validateReading(currentReading, "TS-3", "PIVOT", allPivotAttributeNames, {
                {"GTIS.Identifier", {"string", "M_2367_3_15_6"}},
                {"GTIS.Cause.stVal", {"int64_t", "3"}},
                {"GTIS.TmOrg.stVal", {"string", "substituted"}},
                {"GTIS.SpsTyp.stVal", {"int64_t", "0"}},
                {"GTIS.SpsTyp.t.SecondSinceEpoch", {"int64_t_range", std::to_string(sec-1) + ";" + std::to_string(sec)}},
                {"GTIS.SpsTyp.t.FractionOfSecond", {"int64_t_range", "0;99999999"}},
                {"GTIS.SpsTyp.q.Source", {"string", "substituted"}},
            });
        }
        else {
            validateReading(currentReading, "TS-4", "PIVOT", allPivotAttributeNames, {
                {"GTIS.Identifier", {"string", "M_2367_3_15_7"}},
                {"GTIS.Cause.stVal", {"int64_t", "3"}},
                {"GTIS.TmOrg.stVal", {"string", "substituted"}},
                {"GTIS.DpsTyp.stVal", {"string", "off"}},
                {"GTIS.DpsTyp.t.SecondSinceEpoch", {"int64_t_range", std::to_string(sec-1) + ";" + std::to_string(sec)}},
                {"GTIS.DpsTyp.t.FractionOfSecond", {"int64_t_range", "0;99999999"}},
                {"GTIS.DpsTyp.q.Source", {"string", "substituted"}},
            });
        }
        if(HasFatalFailure()) return;
    }
}

TEST_F(TestSystemSp, CyclicAndConnectionLossMessages)
{
    // Note that in this configuration TS-2 has all known existing pivot_subtypes
    // as this is a supported feature even if it may not always make sense at user level
	static std::string customConfig = QUOTE({
        "enable" :{
            "value": "true"
        },
        "exchanged_data": {
            "value" : {
                "exchanged_data": {
                    "datapoints" : [
                        {
                            "label":"TS-1",
                            "pivot_id":"M_2367_3_15_4",
                            "pivot_type":"SpsTyp",
                            "pivot_subtypes": [
                                "acces"
                            ],
                            "ts_syst_cycle": 3,
                            "protocols":[
                                {
                                    "name":"IEC104",
                                    "typeid":"M_ME_NC_1",
                                    "address":"3271612"
                                }
                            ]
                        },
                        {
                            "label":"TS-2",
                            "pivot_id":"M_2367_3_15_5",
                            "pivot_type":"DpsTyp",
                            "pivot_subtypes": [
                                "acces",
                                "prt.inf",
                                "transient"
                            ],
                            "ts_syst_cycle": 3,
                            "protocols":[
                                {
                                    "name":"IEC104",
                                    "typeid":"M_ME_NC_2",
                                    "address":"3271613"
                                }
                            ]
                        },
                        {
                            "label":"TS-3",
                            "pivot_id":"M_2367_3_15_6",
                            "pivot_type":"SpsTyp",
                            "pivot_subtypes": [
                                "prt.inf"
                            ],
                            "protocols":[
                                {
                                    "name":"IEC104",
                                    "typeid":"M_ME_NC_3",
                                    "address":"3271614"
                                }
                            ]
                        }
                    ]
                }
            }
        }
    });

    debug_print("Reconfigure plugin");
    ASSERT_NO_THROW(plugin_reconfigure(reinterpret_cast<PLUGIN_HANDLE*>(filter), customConfig));
    ASSERT_TRUE(filter->isEnabled());

    // All messages are sent immediately at startup (but sent from threads so still use wait)
    waitUntil(ingestCallbackCalled, 2, 100);
    ASSERT_EQ(ingestCallbackCalled, 2);
    resetCounters();
    // Readings are received in unknown order as they are sent at the same time but from different threads
    std::shared_ptr<Reading> currentReading;
    auto pivotTimestampPair = UtilityPivot::fromTimestamp(UtilityPivot::getCurrentTimestampMs());
    long sec = pivotTimestampPair.first;
    for(int i=0 ; i<2 ; i++) {
        currentReading = popFrontReading();
        ASSERT_NE(nullptr, currentReading.get()) << "Invalid reading at loop " << i;
        if (currentReading->getAssetName() == "TS-1") {
            validateReading(currentReading, "TS-1", "PIVOT", allPivotAttributeNames, {
                {"GTIS.Identifier", {"string", "M_2367_3_15_4"}},
                {"GTIS.Cause.stVal", {"int64_t", "3"}},
                {"GTIS.TmOrg.stVal", {"string", "substituted"}},
                {"GTIS.SpsTyp.stVal", {"int64_t", "1"}},
                {"GTIS.SpsTyp.t.SecondSinceEpoch", {"int64_t_range", std::to_string(sec-1) + ";" + std::to_string(sec)}},
                {"GTIS.SpsTyp.t.FractionOfSecond", {"int64_t_range", "0;99999999"}},
                {"GTIS.SpsTyp.q.Source", {"string", "substituted"}},
            });
        }
        else {
            validateReading(currentReading, "TS-2", "PIVOT", allPivotAttributeNames, {
                {"GTIS.Identifier", {"string", "M_2367_3_15_5"}},
                {"GTIS.Cause.stVal", {"int64_t", "3"}},
                {"GTIS.TmOrg.stVal", {"string", "substituted"}},
                {"GTIS.DpsTyp.stVal", {"string", "on"}},
                {"GTIS.DpsTyp.t.SecondSinceEpoch", {"int64_t_range", std::to_string(sec-1) + ";" + std::to_string(sec)}},
                {"GTIS.DpsTyp.t.FractionOfSecond", {"int64_t_range", "0;99999999"}},
                {"GTIS.DpsTyp.q.Source", {"string", "substituted"}},
            });
        }
        if(HasFatalFailure()) return;
    }

    debug_print("Testing connection connx_status not connected notification");
    // Send notification with reason "connection lost"
    std::string notifConnectionLost = QUOTE({
        "asset": "connx_status",
        "reason": "not connected"
    });
    ASSERT_FALSE(plugin_deliver(reinterpret_cast<PLUGIN_HANDLE*>(filter), "dummyDeliveryName", "dummyNotificationName",
                notifConnectionLost, "dummyMessage"));
    ASSERT_EQ(ingestCallbackCalled, 0);
    resetCounters();

    // Nothing received 1 second after startup
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ASSERT_EQ(ingestCallbackCalled, 0);

    // Nothing received 2 seconds after startup
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ASSERT_EQ(ingestCallbackCalled, 0);

    // TS-1 and TS-2 sent 3 seconds after startup
    debug_print("Wait for TS-1 and TS-2 cyclic call...");
    waitUntil(ingestCallbackCalled, 2, 1100);
    ASSERT_EQ(ingestCallbackCalled, 2);
    resetCounters();

    // Readings are received in unknown order as they are sent at the same time but from different threads
    pivotTimestampPair = UtilityPivot::fromTimestamp(UtilityPivot::getCurrentTimestampMs());
    sec = pivotTimestampPair.first;
    for(int i=0 ; i<2 ; i++) {
        currentReading = popFrontReading();
        ASSERT_NE(nullptr, currentReading.get()) << "Invalid reading at loop " << i;
        if (currentReading->getAssetName() == "TS-1") {
            validateReading(currentReading, "TS-1", "PIVOT", allPivotAttributeNames, {
                {"GTIS.Identifier", {"string", "M_2367_3_15_4"}},
                {"GTIS.Cause.stVal", {"int64_t", "3"}},
                {"GTIS.TmOrg.stVal", {"string", "substituted"}},
                {"GTIS.SpsTyp.stVal", {"int64_t", "1"}},
                {"GTIS.SpsTyp.t.SecondSinceEpoch", {"int64_t_range", std::to_string(sec-1) + ";" + std::to_string(sec)}},
                {"GTIS.SpsTyp.t.FractionOfSecond", {"int64_t_range", "0;99999999"}},
                {"GTIS.SpsTyp.q.Source", {"string", "substituted"}},
            });
        }
        else {
            validateReading(currentReading, "TS-2", "PIVOT", allPivotAttributeNames, {
                {"GTIS.Identifier", {"string", "M_2367_3_15_5"}},
                {"GTIS.Cause.stVal", {"int64_t", "3"}},
                {"GTIS.TmOrg.stVal", {"string", "substituted"}},
                {"GTIS.DpsTyp.stVal", {"string", "on"}},
                {"GTIS.DpsTyp.t.SecondSinceEpoch", {"int64_t_range", std::to_string(sec-1) + ";" + std::to_string(sec)}},
                {"GTIS.DpsTyp.t.FractionOfSecond", {"int64_t_range", "0;99999999"}},
                {"GTIS.DpsTyp.q.Source", {"string", "substituted"}},
            });
        }
        if(HasFatalFailure()) return;
    }

    debug_print("Load empty config");
    ASSERT_NO_THROW(plugin_reconfigure(reinterpret_cast<PLUGIN_HANDLE*>(filter), emptyConfig));
    ASSERT_TRUE(filter->isEnabled());
    // Validate that no message is sent any more after a new config was loaded
    std::this_thread::sleep_for(std::chrono::milliseconds(4000));
    ASSERT_EQ(ingestCallbackCalled, 0);
}

TEST_F(TestSystemSp, PluginDisabled)
{
	static std::string customConfig = QUOTE({
        "enable" :{
            "value": "false"
        },
        "exchanged_data": {
            "value" : {
                "exchanged_data": {
                    "datapoints" : [
                        {
                            "label":"TS-1",
                            "pivot_id":"M_2367_3_15_4",
                            "pivot_type":"SpsTyp",
                            "pivot_subtypes": [
                                "acces"
                            ],
                            "ts_syst_cycle": 3,
                            "protocols":[
                                {
                                    "name":"IEC104",
                                    "typeid":"M_ME_NC_1",
                                    "address":"3271612"
                                }
                            ]
                        },
                        {
                            "label":"TS-3",
                            "pivot_id":"M_2367_3_15_6",
                            "pivot_type":"SpsTyp",
                            "pivot_subtypes": [
                                "prt.inf"
                            ],
                            "protocols":[
                                {
                                    "name":"IEC104",
                                    "typeid":"M_ME_NC_3",
                                    "address":"3271614"
                                }
                            ]
                        }
                    ]
                }
            }
        }
    });

    debug_print("Reconfigure plugin");
    ASSERT_NO_THROW(plugin_reconfigure(reinterpret_cast<PLUGIN_HANDLE*>(filter), customConfig));
    ASSERT_FALSE(filter->isEnabled());

    // Nothing is sent when plugin disabled
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(ingestCallbackCalled, 0);

    debug_print("Testing not connected notification");
    // Send notification with reason "not connected" (ignored as plugin disabled)
    std::string notifConnectionLost = QUOTE({
        "asset": "connx_status",
        "reason": "not connected"
    });
    ASSERT_FALSE(plugin_deliver(reinterpret_cast<PLUGIN_HANDLE*>(filter), "dummyDeliveryName", "dummyNotificationName",
                notifConnectionLost, "dummyMessage"));
    ASSERT_EQ(ingestCallbackCalled, 0);

    // Nothing received 3 seconds after startup
    debug_print("Waiting for ignored cyclic call...");
    std::this_thread::sleep_for(std::chrono::milliseconds(3100));
    ASSERT_EQ(ingestCallbackCalled, 0);
}


TEST_F(TestSystemSp, GetMessageTemplate)
{
    const std::string defaultMessageTemplate = QUOTE({
        "PIVOT": {
            "GTIS": {
                "Identifier": "<pivot_id>",
                "Cause": {
                    "stVal": 3
                },
                "<pivot_type>": {
                    "stVal": <value>,
                    "q": {
                        "Source": "substituted"
                    },
                    "t": {
                        "SecondSinceEpoch": <timestamp_sec>,
                        "FractionOfSecond": <timestamp_sub_sec>
                    }
                },
                "TmOrg": {
                    "stVal": "substituted"
                }
            }
        }
    });
    ASSERT_STREQ(filter->getMessageTemplate("invalid").c_str(), "");
    ASSERT_STREQ(filter->getMessageTemplate("acces").c_str(), defaultMessageTemplate.c_str());
    ASSERT_STREQ(filter->getMessageTemplate("prt.inf").c_str(), defaultMessageTemplate.c_str());
}

TEST_F(TestSystemSp, InvalidPivotType)
{
    // Load a config with no TS
    static std::string customConfig = QUOTE({
        "enable" :{
            "value": "true"
        },
        "exchanged_data": {
            "value" : {
                "exchanged_data": {
                    "datapoints" : []
                }
            }
        }
    });

    debug_print("Reconfigure plugin");
    ASSERT_NO_THROW(plugin_reconfigure(reinterpret_cast<PLUGIN_HANDLE*>(filter), customConfig));
    ASSERT_TRUE(filter->isEnabled());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(ingestCallbackCalled, 0);

    // Manually add erroneous configuration for a TS with "acces" and "prt.inf" and an invalid pivot type
    // During a regular config import, this is prevented by ConfigPlugin::m_importDatapoint()
    // as messages with unexpected pivot type are ignored by it
    filter->getConfigPlugin().addDataInfo("acces", std::make_shared<CyclicDataInfo>("invalid", "invalid", "invalid", 1));
    filter->getConfigPlugin().addDataInfo("prt.inf", std::make_shared<DataInfo>("invalid", "invalid", "invalid"));

    // Restart the cycles to take manual config into account
    debug_print("Restart cycles");
    ASSERT_NO_THROW(filter->stopCycles());
    ASSERT_NO_THROW(filter->startCycles());

    // Let the cycle loop a few times and check that no message was sent
    debug_print("Wait for cycles execution...");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    ASSERT_EQ(ingestCallbackCalled, 0);

    // Send notification with reason "not connected" (ignored as invaid pivot type)
    std::string notifConnectionLost = QUOTE({
        "asset": "connx_status",
        "reason": "not connected"
    });
    ASSERT_FALSE(plugin_deliver(reinterpret_cast<PLUGIN_HANDLE*>(filter), "dummyDeliveryName", "dummyNotificationName",
                notifConnectionLost, "dummyMessage"));
    ASSERT_EQ(ingestCallbackCalled, 0);
}