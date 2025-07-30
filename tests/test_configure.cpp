#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <plugin_api.h>

#include "notifySystemSp.h"

using namespace systemspn;

static std::string configureOKSps = QUOTE({
    "exchanged_data": {
        "datapoints" : [
            {
                "label":"TS-1",
                "pivot_id":"M_2367_3_15_4",
                "pivot_type":"SpsTyp",
                "pivot_subtypes": [
                     "acces"
                ],
                "ts_syst_cycle" :30,
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
                "pivot_type":"SpsTyp",
                "pivot_subtypes": [
                    "prt.inf",
                    "transient"
                ],
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
});

static std::string configureOKDps = QUOTE({
    "exchanged_data": {
        "datapoints" : [
            {
                "label":"TS-1",
                "pivot_id":"M_2367_3_15_4",
                "pivot_type":"DpsTyp",
                "pivot_subtypes": [
                    "acces"
                ],
                "ts_syst_cycle" :30,
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
                    "prt.inf"
                ],
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
});

extern "C" {
	PLUGIN_INFORMATION *plugin_info();
	PLUGIN_HANDLE plugin_init(ConfigCategory *config);
    void plugin_shutdown(PLUGIN_HANDLE *handle);
};

class TestPluginConfigure : public testing::Test
{
protected:
    NotifySystemSp *filter = nullptr;  // Object on which we call for tests
    ConfigCategory *config = nullptr;

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
        ASSERT_EQ(filter->isEnabled(), true);
    }

    // TearDown is ran for every tests, so each variable are destroyed again
    void TearDown() override
    {
        if (filter) {
            ASSERT_NO_THROW(plugin_shutdown(reinterpret_cast<PLUGIN_HANDLE*>(filter)));
        }
    }

    static std::map<std::string, std::string> expectedPivotIds;
    static std::map<std::string, std::string> expectedAssetNames;
};

std::map<std::string, std::string> TestPluginConfigure::expectedPivotIds = {
    {"acces", "M_2367_3_15_4"},
    {"prt.inf", "M_2367_3_15_5"},
    {"transient", "M_2367_3_15_5"}
};
std::map<std::string, std::string> TestPluginConfigure::expectedAssetNames = {
    {"acces", "TS-1"},
    {"prt.inf", "TS-2"},
    {"transient", "TS-2"}
};

TEST_F(TestPluginConfigure, ConfigureErrorParsingJSON)
{
	std::string configureErrorParseJSON = QUOTE({
        "exchanged_data" : {
            "eee"
        }
    });

    filter->setJsonConfig(configureErrorParseJSON);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorRootNotObject)
{
	std::string configureErrorRootNotObject = QUOTE(42);

    filter->setJsonConfig(configureErrorRootNotObject);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorNoExchangedData)
{
	std::string configureErrorNoExchangedData = QUOTE({});

    filter->setJsonConfig(configureErrorNoExchangedData);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorExchangedDataNotObject)
{
	std::string configureErrorExchangedDataNotObject = QUOTE({
        "exchanged_data" : 42
    });

    filter->setJsonConfig(configureErrorExchangedDataNotObject);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorNoDatapoints)
{
	std::string configureErrorNoDatapoints = QUOTE({
        "exchanged_data": {}
    });

    filter->setJsonConfig(configureErrorNoDatapoints);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorDatapointsNotArray)
{
	std::string configureErrorDatapointsNotArray = QUOTE({
        "exchanged_data": {
            "datapoints" : 42
        }
    });

    filter->setJsonConfig(configureErrorDatapointsNotArray);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorDatapointsNotContainsObject)
{
	std::string configureErrorDatapointsNotContainsObject = QUOTE({
        "exchanged_data": {
            "datapoints" : [
                42
            ]
        }
    });

    filter->setJsonConfig(configureErrorDatapointsNotContainsObject);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorNoType)
{
	std::string configureErrorNoType = QUOTE({
        "exchanged_data": {
            "datapoints" : [
                {
                    "label":"TS-1",
                    "pivot_id":"M_2367_3_15_4",
                    "pivot_subtypes": [
                        "prt.inf"
                    ],
                    "protocols":[
                        {
                            "name":"IEC104",
                            "typeid":"M_ME_NC_1",
                            "address":"3271612"
                        }
                    ]
                }
            ]
        }
    });

    filter->setJsonConfig(configureErrorNoType);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorTypeNotString)
{
	std::string configureErrorTypeNotString = QUOTE({
        "exchanged_data": {
            "datapoints" : [
                {
                    "label":"TS-1",
                    "pivot_id":"M_2367_3_15_4",
                    "pivot_type": 42,
                    "pivot_subtypes": [
                        "prt.inf"
                    ],
                    "protocols":[
                        {
                            "name":"IEC104",
                            "typeid":"M_ME_NC_1",
                            "address":"3271612"
                        }
                    ]
                }
            ]
        }
    });

    filter->setJsonConfig(configureErrorTypeNotString);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorInvalidType)
{
	std::string configureErrorInvalidType = QUOTE({
        "exchanged_data": {
            "datapoints" : [
                {
                    "label":"TS-1",
                    "pivot_id":"M_2367_3_15_4",
                    "pivot_type":"MvTyp",
                    "pivot_subtypes": [
                        "prt.inf"
                    ],
                    "protocols":[
                        {
                            "name":"IEC104",
                            "typeid":"M_ME_NC_1",
                            "address":"3271612"
                        }
                    ]
                }
            ]
        }
    });

    filter->setJsonConfig(configureErrorInvalidType);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorNoPivotID)
{
	std::string configureErrorNoPivotID = QUOTE({
        "exchanged_data": {
            "datapoints" : [
                {
                    "label":"TS-1",
                    "pivot_type":"SpsTyp",
                    "pivot_subtypes": [
                        "prt.inf"
                    ],
                    "protocols":[
                        {
                            "name":"IEC104",
                            "typeid":"M_ME_NC_1",
                            "address":"3271612"
                        }
                    ]
                }
            ]
        }
    });

    filter->setJsonConfig(configureErrorNoPivotID);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorPivotIDNotString)
{
	std::string configureErrorPivotIDNotString = QUOTE({
        "exchanged_data": {
            "datapoints" : [
                {
                    "label":"TS-1",
                    "pivot_id": 42,
                    "pivot_type":"SpsTyp",
                    "pivot_subtypes": [
                        "prt.inf"
                    ],
                    "protocols":[
                        {
                            "name":"IEC104",
                            "typeid":"M_ME_NC_1",
                            "address":"3271612"
                        }
                    ]
                }
            ]
        }
    });

    filter->setJsonConfig(configureErrorPivotIDNotString);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorNoLabel)
{
	std::string configureErrorNoLabel = QUOTE({
        "exchanged_data": {
            "datapoints" : [
                {
                    "pivot_id":"M_2367_3_15_4",
                    "pivot_type":"SpsTyp",
                    "pivot_subtypes": [
                        "prt.inf"
                    ],
                    "protocols":[
                        {
                            "name":"IEC104",
                            "typeid":"M_ME_NC_1",
                            "address":"3271612"
                        }
                    ]
                }
            ]
        }
    });

    filter->setJsonConfig(configureErrorNoLabel);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorLabelNotString)
{
	std::string configureErrorLabelNotString = QUOTE({
        "exchanged_data": {
            "datapoints" : [
                {
                    "label": 42,
                    "pivot_id":"M_2367_3_15_4",
                    "pivot_type":"SpsTyp",
                    "pivot_subtypes": [
                        "prt.inf"
                    ],
                    "protocols":[
                        {
                            "name":"IEC104",
                            "typeid":"M_ME_NC_1",
                            "address":"3271612"
                        }
                    ]
                }
            ]
        }
    });

    filter->setJsonConfig(configureErrorLabelNotString);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorNoSubtypes)
{
	std::string configureErrorNoSubtypes = QUOTE({
        "exchanged_data": {
            "datapoints" : [
                {
                    "label":"TS-1",
                    "pivot_id":"M_2367_3_15_4",
                    "pivot_type":"SpsTyp",
                    "protocols":[
                        {
                            "name":"IEC104",
                            "typeid":"M_ME_NC_1",
                            "address":"3271612"
                        }
                    ]
                }
            ]
        }
    });

    filter->setJsonConfig(configureErrorNoSubtypes);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorSubtypesNotArray)
{
	std::string configureErrorSubtypesNotArray = QUOTE({
        "exchanged_data": {
            "datapoints" : [
                {
                    "label":"TS-1",
                    "pivot_id":"M_2367_3_15_4",
                    "pivot_type":"SpsTyp",
                    "pivot_subtypes": 42,
                    "protocols":[
                        {
                            "name":"IEC104",
                            "typeid":"M_ME_NC_1",
                            "address":"3271612"
                        }
                    ]
                }
            ]
        }
    });

    filter->setJsonConfig(configureErrorSubtypesNotArray);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorSubtypesNotContainString)
{
	std::string configureErrorSubtypesNotContainString = QUOTE({
        "exchanged_data": {
            "datapoints" : [
                {
                    "label":"TS-1",
                    "pivot_id":"M_2367_3_15_4",
                    "pivot_type":"SpsTyp",
                    "pivot_subtypes": [
                        42
                    ],
                    "protocols":[
                        {
                            "name":"IEC104",
                            "typeid":"M_ME_NC_1",
                            "address":"3271612"
                        }
                    ]
                }
            ]
        }
    });

    filter->setJsonConfig(configureErrorSubtypesNotContainString);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorSubtypesWithUnknownSubtype)
{
	std::string configureErrorSubtypesWithUnknownSubtype = QUOTE({
        "exchanged_data": {
            "datapoints" : [
                {
                    "label":"TS-1",
                    "pivot_id":"M_2367_3_15_4",
                    "pivot_type":"SpsTyp",
                    "pivot_subtypes": [
                        "test"
                    ],
                    "protocols":[
                        {
                            "name":"IEC104",
                            "typeid":"M_ME_NC_1",
                            "address":"3271612"
                        }
                    ]
                }
            ]
        }
    });

    filter->setJsonConfig(configureErrorSubtypesWithUnknownSubtype);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorSubtypesWithMissingCycle)
{
	std::string configureErrorSubtypesWithMissingCycle = QUOTE({
        "exchanged_data": {
            "datapoints" : [
                {
                    "label":"TS-1",
                    "pivot_id":"M_2367_3_15_4",
                    "pivot_type":"SpsTyp",
                    "pivot_subtypes": [
                        "acces"
                    ],
                    "protocols":[
                        {
                            "name":"IEC104",
                            "typeid":"M_ME_NC_1",
                            "address":"3271612"
                        }
                    ]
                }
            ]
        }
    });

    filter->setJsonConfig(configureErrorSubtypesWithMissingCycle);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureErrorCycleNotInt)
{
	std::string configureErrorCycleNotInt = QUOTE({
        "exchanged_data": {
            "datapoints" : [
                {
                    "label":"TS-1",
                    "pivot_id":"M_2367_3_15_4",
                    "pivot_type":"SpsTyp",
                    "pivot_subtypes": [
                        "acces"
                    ],
                    "ts_syst_cycle":"zzz",
                    "protocols":[
                        {
                            "name":"IEC104",
                            "typeid":"M_ME_NC_1",
                            "address":"3271612"
                        }
                    ]
                }
            ]
        }
    });

    filter->setJsonConfig(configureErrorCycleNotInt);
    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        ASSERT_EQ(dataSystem[dataType].size(), 0) << "No " << dataType << " data should be stored";
    }
}

TEST_F(TestPluginConfigure, ConfigureOKSps)
{
	filter->setJsonConfig(configureOKSps);
    const auto& dataTypes = filter->getConfigPlugin().getDataTypes();
    const auto& dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        // Transient data is not stored in the data system
        if( dataType != "transient") {
            ASSERT_EQ(dataSystem.at(dataType).size(), 1) << "Unexpected number of " << dataType << " stored";
            ASSERT_TRUE(filter->getConfigPlugin().hasDataForType(dataType, expectedPivotIds[dataType]))
                << "No rule found for type " << dataType << " and pivot_id " << expectedPivotIds[dataType];
            const auto& dataInfo = dataSystem.at(dataType).at(0);
            ASSERT_NE(dataInfo.get(), nullptr) << "Data of type " << dataType << " is null";
            DataInfo* tmp = nullptr;
            if (dataType == "acces") {
                tmp = dynamic_cast<CyclicDataInfo*>(dataInfo.get());
            }
            else if (dataType == "prt.inf") {
                tmp = dynamic_cast<DataInfo*>(dataInfo.get());
            }
            ASSERT_NE(tmp, nullptr) << "Wrong object class stored for type " << dataType;
            ASSERT_STREQ(dataInfo->pivotId.c_str(), expectedPivotIds[dataType].c_str())
                << "Unexpected pivot ID "<< dataInfo->pivotId << " for type " << dataType;
            ASSERT_STREQ(dataInfo->pivotType.c_str(), "SpsTyp")
                << "Unexpected pivot type "<< dataInfo->pivotType << " for type " << dataType;
            ASSERT_STREQ(dataInfo->assetName.c_str(), expectedAssetNames[dataType].c_str())
                << "Unexpected asset name "<< dataInfo->assetName << " for type " << dataType;
            if (dataType == "acces") {
                auto cyclicDataInfo = std::dynamic_pointer_cast<CyclicDataInfo>(dataInfo);
                ASSERT_EQ(cyclicDataInfo->cycleSec, 30)
                    << "Unexpected cycle seconds " << cyclicDataInfo->cycleSec << " for type " << dataType;
            }
        }
    }
}

TEST_F(TestPluginConfigure, ConfigureOKDps)
{
	filter->setJsonConfig(configureOKDps);
    const auto& dataTypes = filter->getConfigPlugin().getDataTypes();
    const auto& dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());
    for(const auto& dataType : dataTypes) {
        ASSERT_EQ(dataSystem.count(dataType), 1) << "Missing data type " << dataType;
        // Transient data is not stored in the data system
        if( dataType != "transient") {
            ASSERT_EQ(dataSystem.at(dataType).size(), 1) << "Unexpected number of " << dataType << " stored";
            ASSERT_TRUE(filter->getConfigPlugin().hasDataForType(dataType, expectedPivotIds[dataType]))
                << "No rule found for type " << dataType << " and pivot_id " << expectedPivotIds[dataType];
            const auto& dataInfo = dataSystem.at(dataType).at(0);
            ASSERT_NE(dataInfo.get(), nullptr) << "Data of type " << dataType << " is null";
            DataInfo* tmp = nullptr;
            if (dataType == "acces") {
                tmp = dynamic_cast<CyclicDataInfo*>(dataInfo.get());
            }
            else if (dataType == "prt.inf") {
                tmp = dynamic_cast<DataInfo*>(dataInfo.get());
            }
            ASSERT_NE(tmp, nullptr) << "Wrong object class stored for type " << dataType;
            ASSERT_STREQ(dataInfo->pivotId.c_str(), expectedPivotIds[dataType].c_str())
                << "Unexpected pivot ID "<< dataInfo->pivotId << " for type " << dataType;
            ASSERT_STREQ(dataInfo->pivotType.c_str(), "DpsTyp")
                << "Unexpected pivot type "<< dataInfo->pivotType << " for type " << dataType;
            ASSERT_STREQ(dataInfo->assetName.c_str(), expectedAssetNames[dataType].c_str())
                << "Unexpected asset name "<< dataInfo->assetName << " for type " << dataType;
            if (dataType == "acces") {
                auto cyclicDataInfo = std::dynamic_pointer_cast<CyclicDataInfo>(dataInfo);
                ASSERT_EQ(cyclicDataInfo->cycleSec, 30)
                    << "Unexpected cycle seconds " << cyclicDataInfo->cycleSec << " for type " << dataType;
            }
        }
    }
}

TEST_F(TestPluginConfigure, HasDataForTypeInvalidType)
{
	filter->setJsonConfig(configureOKSps);
    ASSERT_FALSE(filter->getConfigPlugin().hasDataForType("invalid_type", "M_2367_3_15_4"));
}

TEST_F(TestPluginConfigure, ConfigurePrtInf)
{
	std::string configuration = QUOTE({
        "exchanged_data": {
            "datapoints" : [
                {
                    "label":"TS-OK",
                    "pivot_id":"M_2367_3_15_6",
                    "pivot_type":"SpsTyp",
                    "pivot_subtypes": [
                        "prt.inf",
                        "transient"
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
                    "label":"TS-WARNING",
                    "pivot_id":"M_2367_3_15_4",
                    "pivot_type":"SpsTyp",
                    "pivot_subtypes": [
                        "prt.inf"
                    ],
                    "protocols":[
                        {
                            "name":"IEC104",
                            "typeid":"M_ME_NC_1",
                            "address":"3271612"
                        }
                    ]
                }
            ]
        }
    });
    filter->setJsonConfig(configuration);

    auto dataTypes = filter->getConfigPlugin().getDataTypes();
    auto dataSystem = filter->getConfigPlugin().getDataSystem();
    ASSERT_EQ(dataTypes.size(), expectedPivotIds.size());

    ASSERT_EQ(dataSystem.count("prt.inf"), 1) << "Missing data type prt.inf";
    auto di_list = dataSystem.at("prt.inf");
    ASSERT_EQ(di_list.size(), 2) << "Unexpected number of prt.inf data stored";
    for (const auto& di : di_list) {
        if (di->pivotId == "M_2367_3_15_6") {
            ASSERT_FALSE(di->isTransientWarning);
        }
        if (di->pivotId == "M_2367_3_15_4") {
            ASSERT_TRUE(di->isTransientWarning);
        }
    }

}