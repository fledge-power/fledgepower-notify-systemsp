#include <logger.h>
#include <cctype>
#include <algorithm>
#include <set>

#include "configPlugin.h"
#include "constantsSystem.h"
#include "utilityPivot.h"

using namespace systemspn;

/**
 * Constructor
*/
ConfigPlugin::ConfigPlugin() {
    m_reset();
}

/**
 * Import data in the form of Exchanged_data
 * The data is saved in a map m_exchangeDefinitions
 * 
 * @param exchangeConfig : configuration Exchanged_data as a string 
*/
void ConfigPlugin::importExchangedData(const std::string & exchangeConfig) {
    
    std::string beforeLog = ConstantsSystem::NamePlugin + " - ConfigPlugin::importExchangedData :";
    rapidjson::Document document;

    m_reset();

    if (document.Parse(exchangeConfig.c_str()).HasParseError()) {
        UtilityPivot::log_fatal("%s Parsing error in data exchange configuration", beforeLog.c_str());
        return;
    }

    if (!document.IsObject()) {
        UtilityPivot::log_fatal("%s Root element is not an object", beforeLog.c_str());
        return;
    }

    if (!document.HasMember(ConstantsSystem::JsonExchangedData) || !document[ConstantsSystem::JsonExchangedData].IsObject()) {
        UtilityPivot::log_fatal("%s exchanged_data not found in root object or is not an object", beforeLog.c_str());
        return;
    }
    const rapidjson::Value& exchangeData = document[ConstantsSystem::JsonExchangedData];

    if (!exchangeData.HasMember(ConstantsSystem::JsonDatapoints) || !exchangeData[ConstantsSystem::JsonDatapoints].IsArray()) {
        UtilityPivot::log_fatal("%s datapoints not found in exchanged_data or is not an array", beforeLog.c_str());
        return;
    }
    const rapidjson::Value& datapoints = exchangeData[ConstantsSystem::JsonDatapoints];

    for (const rapidjson::Value& datapoint : datapoints.GetArray()) {
        m_importDatapoint(datapoint);
    }
}

/**
 * Import data from a single datapoint of exchanged data
 * 
 * @param datapoint : datapoint to parse and import
*/
void ConfigPlugin::m_importDatapoint(const rapidjson::Value& datapoint) {
    std::string beforeLog = ConstantsSystem::NamePlugin + " - ConfigPlugin::m_importDatapoint :";
    if (!datapoint.IsObject()) {
        UtilityPivot::log_error("%s datapoint is not an object", beforeLog.c_str());
        return;
    }
    
    if (!datapoint.HasMember(ConstantsSystem::JsonPivotType) || !datapoint[ConstantsSystem::JsonPivotType].IsString()) {
        UtilityPivot::log_error("%s pivot_type not found in datapoint or is not a string", beforeLog.c_str());
        return;
    }

    std::string type = datapoint[ConstantsSystem::JsonPivotType].GetString();
    if (type != ConstantsSystem::JsonCdcSps && type != ConstantsSystem::JsonCdcDps) {
        // Ignore datapoints that are not a TS
        return;
    }

    if (!datapoint.HasMember(ConstantsSystem::JsonPivotId) || !datapoint[ConstantsSystem::JsonPivotId].IsString()) {
        UtilityPivot::log_error("%s pivot_id not found in datapoint or is not a string", beforeLog.c_str());
        return;
    }
    std::string pivot_id = datapoint[ConstantsSystem::JsonPivotId].GetString();

    if (!datapoint.HasMember(ConstantsSystem::JsonPivotSubtypes) || !datapoint[ConstantsSystem::JsonPivotSubtypes].IsArray()) {
        // No pivot subtypes, nothing to do
        return;
    }

    if (!datapoint.HasMember(ConstantsSystem::JsonLabel) || !datapoint[ConstantsSystem::JsonLabel].IsString()) {
        UtilityPivot::log_error("%s label not found in datapoint or is not a string", beforeLog.c_str());
        return;
    }
    std::string label = datapoint[ConstantsSystem::JsonLabel].GetString();

    std::set<std::string> foundConfigs;
    auto subtypes = datapoint[ConstantsSystem::JsonPivotSubtypes].GetArray();

    for (rapidjson::Value::ConstValueIterator itr = subtypes.Begin(); itr != subtypes.End(); ++itr) {
        if (!(*itr).IsString()) {
            continue;
        }
        std::string s = (*itr).GetString();
        for(const auto& dataType: m_allDataTypes) {
            if(s == dataType) {
                foundConfigs.insert(dataType);
            }
        }
    }

    if (foundConfigs.count("acces") > 0) {
        if (!datapoint.HasMember(ConstantsSystem::JsonTsSystCycle) || !datapoint[ConstantsSystem::JsonTsSystCycle].IsInt()) {
            UtilityPivot::log_error("%s Configuration access on %s, but no %s found", beforeLog.c_str(), label.c_str(), ConstantsSystem::JsonTsSystCycle);
        }
        else {
            int cycle_s = datapoint[ConstantsSystem::JsonTsSystCycle].GetInt();
            addDataInfo("acces", std::make_shared<CyclicDataInfo>(pivot_id, type, label, cycle_s));
            UtilityPivot::log_debug("%s Configuration access on %s : [%s, %s, %d]",
                                    beforeLog.c_str(), label.c_str(), pivot_id.c_str(), type.c_str(), cycle_s);
        }
    }
    
    if (foundConfigs.count("prt.inf") > 0) {
        addDataInfo("prt.inf", std::make_shared<DataInfo>(pivot_id, type, label));
        UtilityPivot::log_debug("%s Configuration prt.inf on %s : [%s, %s]",
                                    beforeLog.c_str(), label.c_str(), pivot_id.c_str(), type.c_str());
    }
}

/**
 * Tells if there is currently a data info stored for the given type and pivot ID
 * 
 * @param dataType Type of data to look for
 * @param pivotId Pivot ID to look for
 * @return True if a result is found, else false
*/
bool ConfigPlugin::hasDataForType(const std::string& dataType, const std::string& pivotId) const {
    std::string beforeLog = ConstantsSystem::NamePlugin + " - ConfigPlugin::hasDataForType :";
    if (m_dataSystem.count(dataType) == 0) {
        UtilityPivot::log_error("%s Invalid dataType: %s", beforeLog.c_str(), dataType.c_str());
        return false;
    }
    const auto& dataSystem = m_dataSystem.at(dataType);
    return std::find_if(dataSystem.begin(), dataSystem.end(),
        [&pivotId](std::shared_ptr<DataInfo> di){ return di->pivotId == pivotId; }) != dataSystem.end();
}

/**
 * Adds information about a TI that can be sent by the plugin
 * 
 * @param dataType Type of data to add
 * @param dataInfo Information about that TI
*/
void ConfigPlugin::addDataInfo(const std::string& dataType, std::shared_ptr<DataInfo> dataInfo) {
    m_dataSystem[dataType].push_back(dataInfo);
}

/**
 * Reset the data stored for all types and define all expected types in the map
*/
void ConfigPlugin::m_reset() {
    // Clears the map
    m_dataSystem.clear();
    // Creates the expected entries in it
    for(const auto& dataType: m_allDataTypes) {
        m_dataSystem[dataType];
    }
}