#include <logger.h>
#include <cctype>
#include <algorithm>
#include <set>

#include <rapidjson/document.h>

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
        
        if (!datapoint.IsObject()) {
            UtilityPivot::log_error("%s datapoint is not an object", beforeLog.c_str());
            continue;
        }
        
        if (!datapoint.HasMember(ConstantsSystem::JsonPivotType) || !datapoint[ConstantsSystem::JsonPivotType].IsString()) {
            UtilityPivot::log_error("%s pivot_type not found in datapoint or is not a string", beforeLog.c_str());
            continue;
        }

        std::string type = datapoint[ConstantsSystem::JsonPivotType].GetString();
        if (type != ConstantsSystem::JsonCdcSps && type != ConstantsSystem::JsonCdcDps) {
            // Ignore datapoints that are not a TS
            continue;
        }

        if (!datapoint.HasMember(ConstantsSystem::JsonPivotId) || !datapoint[ConstantsSystem::JsonPivotId].IsString()) {
            UtilityPivot::log_error("%s pivot_id not found in datapoint or is not a string", beforeLog.c_str());
            continue;
        }
        std::string pivot_id = datapoint[ConstantsSystem::JsonPivotId].GetString();

        if (!datapoint.HasMember(ConstantsSystem::JsonPivotSubtypes) || !datapoint[ConstantsSystem::JsonPivotSubtypes].IsArray()) {
            // No pivot subtypes, nothing to do
            continue;
        }

        if (!datapoint.HasMember(ConstantsSystem::JsonLabel) || !datapoint[ConstantsSystem::JsonLabel].IsString()) {
            UtilityPivot::log_error("%s label not found in datapoint or is not a string", beforeLog.c_str());
            continue;
        }
        std::string label = datapoint[ConstantsSystem::JsonLabel].GetString();

        std::set<std::string> foundConfigs;
        auto subtypes = datapoint[ConstantsSystem::JsonPivotSubtypes].GetArray();

        for (rapidjson::Value::ConstValueIterator itr = subtypes.Begin(); itr != subtypes.End(); ++itr) {
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
                m_dataSystem["acces"].push_back(std::make_shared<CyclicDataInfo>(pivot_id, type, label, cycle_s));
                UtilityPivot::log_debug("%s Configuration access on %s : [%s, %s, %d]",
                                        beforeLog.c_str(), label.c_str(), pivot_id.c_str(), type.c_str(), cycle_s);
            }
        }
        
        if (foundConfigs.count("prt.inf") > 0) {
            m_dataSystem["prt.inf"].push_back(std::make_shared<DataInfo>(pivot_id, type, label));
            UtilityPivot::log_debug("%s Configuration prt.inf on %s : [%s, %s]",
                                        beforeLog.c_str(), label.c_str(), pivot_id.c_str(), type.c_str());
        }
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
        [pivotId](std::shared_ptr<DataInfo> di){ return di->pivotId == pivotId; }) != dataSystem.end();
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