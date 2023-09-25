/*
 * Fledge filter sets value back to 0 for system status point
 *
 * Copyright (c) 2020, RTE (https://www.rte-france.com)
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Yannick Marchetaux
 * 
 */
#include <regex>
#include <datapoint.h>
#include <reading.h>
#include <plugin_api.h>
#include <rapidjson/document.h>

#include "notifySystemSp.h"
#include "constantsSystem.h"
#include "datapoint_utility.h"
#include "utilityPivot.h"

using namespace DatapointUtility;
using namespace systemspn;

/**
 * Constructor for the NotifySystemSp class.
 */
NotifySystemSp::NotifySystemSp() {}

/**
 * Destructor for the NotifySystemSp class.
 */
NotifySystemSp::~NotifySystemSp() {
    stopCycles();
}

/**
 * Modification of configuration
 * 
 * @param jsonExchanged : configuration ExchangedData
 */
void NotifySystemSp::setJsonConfig(const std::string& jsonExchanged) {
    m_configPlugin.importExchangedData(jsonExchanged);
    // Reinitialize cyclic messages
    startCycles();
}

/**
 * Get the json template of a reading for the given data type
 * 
 * @param dataType : Type of status point
 * @return Json reading template
 */
std::string NotifySystemSp::getMessageTemplate(const std::string& dataType) const {
    std::string beforeLog = ConstantsSystem::NamePlugin + " - NotifySystemSp::getMessageTemplate : ";
    if ((dataType == "acces") || (dataType == "prt.inf")) {
        return QUOTE({
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
    }
    else {
        UtilityPivot::log_fatal("%s Invalid data type: %s", beforeLog.c_str(), dataType.c_str());
        return "";
    }
}

/**
 * For each cyclic status point in the configuration, starts the cycle to send it periodically
 */
void NotifySystemSp::startCycles() {
    std::string beforeLog = ConstantsSystem::NamePlugin + " - NotifySystemSp::startCycles : ";
    UtilityPivot::log_debug("%s Starting configured cycles...", beforeLog.c_str());

    // If any cycle was already in progress, stop them
    stopCycles();

    // Start a new cycle thread for each cyclic status point
    m_isRunning = true;
    std::string messageTemplate = getMessageTemplate("acces");
    const auto& dataSystem = m_configPlugin.getDataSystem();
    for(const auto& dataInfo : dataSystem.at("acces")) {
        // All data infos from access status points are cyclic ones
        auto cyclicDataInfo = std::dynamic_pointer_cast<CyclicDataInfo>(dataInfo);
        m_cycleThreads.push_back(
            std::thread(&NotifySystemSp::runCycles, this, messageTemplate, cyclicDataInfo->pivotId, cyclicDataInfo->pivotType,
                        cyclicDataInfo->assetName, cyclicDataInfo->cycleSec)
        );
    }

    UtilityPivot::log_debug("%s Cycles started!", beforeLog.c_str());
}

/**
 * Stops all status point emission cycles
 */
void NotifySystemSp::stopCycles() {
    std::string beforeLog = ConstantsSystem::NamePlugin + " - NotifySystemSp::stopCycles : ";
    UtilityPivot::log_debug("%s Stopping all existing cycles...", beforeLog.c_str());

    m_isRunning = false;
    for(auto& thread: m_cycleThreads) {
        thread.join();
    }
    m_cycleThreads.clear();

    UtilityPivot::log_debug("%s Cycles stopped!", beforeLog.c_str());
}

/**
 * Thread function used to send one status point periodically
 * 
 * @param messageTemplate String containing the template of the reading json message to send
 * @param pivotId Pivot ID to use in the message
 * @param pivotType Pivot Type to use in the message
 * @param assetName Asset name to use for the message
 * @param cycleSec Period in seconds when to send the message
 */
void NotifySystemSp::runCycles(const std::string& messageTemplate, const std::string& pivotId,
                               const std::string& pivotType, const std::string& assetName, int cycleSec) {
    long lastMessageTimeMs = 0;
    long cycleMs = 1000L * cycleSec;

    std::string beforeLog = ConstantsSystem::NamePlugin + " - NotifySystemSp::runCycles : ";
    UtilityPivot::log_debug("%s Status Point cycle thread running for %s", beforeLog.c_str(), assetName.c_str());

    while (m_isRunning) {
        if (!isEnabled()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        // Send the message
        long currentTimeMs = UtilityPivot::getCurrentTimestampMs();
        long timeSinceLastSent = currentTimeMs - lastMessageTimeMs;
        long timeRemaining = cycleMs - timeSinceLastSent;
        if (timeRemaining <= 0) {
            // Fill the template with variable values
            std::string jsonReading = fillTemplate(messageTemplate, pivotId, pivotType, currentTimeMs);
            if (jsonReading.size() == 0) {
                return;
            }
            // Send a reading with data from the template
            sendReading(assetName, jsonReading);
            lastMessageTimeMs = currentTimeMs;
            timeRemaining = cycleMs;
        }
        // Sleep for 1 second or less (avoids waiting for full cycle time when thread is stopping)
        std::this_thread::sleep_for(std::chrono::milliseconds(std::min(timeRemaining, 1000L)));
    }

    UtilityPivot::log_debug("%s Status Point cycle thread stopped for %s", beforeLog.c_str(), assetName.c_str());
}

/**
 * Send the reading data of a status point
 * 
 * @param assetName Name of the asset that will contain the reading
 * @param jsonReading Json string representing the reading to send
 */
void NotifySystemSp::sendReading(const std::string& assetName, const std::string& jsonReading) {
    std::string beforeLog = ConstantsSystem::NamePlugin + " - NotifySystemSp::sendReading : ";
    UtilityPivot::log_debug("%s Creating and sending asset '%s' with reading %s", beforeLog.c_str(), assetName.c_str(), jsonReading.c_str());
    // Dummy object used to be able to call parseJson() freely
    static DatapointValue dummyValue("");
    static Datapoint dummyDataPoint({}, dummyValue);
    // Send the message
    auto datapoints = dummyDataPoint.parseJson(jsonReading);
    Reading reading(assetName, *datapoints);
    ingest(reading);
}

/**
 * Generate a reading json from a template and values
 * 
 * @param messageTemplate String containing the template of the reading json message to send
 * @param pivotId Pivot ID to use in the message
 * @param pivotType Pivot Type to use in the message
 * @param timestampMs Timestamp in ms to use in the message
 * @param on Value of the message (True = 1/"on", False = 0/"off")
 */
std::string NotifySystemSp::fillTemplate(const std::string& messageTemplate, const std::string& pivotId,
                                         const std::string& pivotType, long timestampMs, bool on /*= true*/) const {
    std::string beforeLog = ConstantsSystem::NamePlugin + " - " + pivotId + " - NotifySystemSp::fillTemplate : ";
    // Fill the template with variable values
    std::string message = std::regex_replace(messageTemplate, std::regex("<pivot_id>"), pivotId);
    message = std::regex_replace(message, std::regex("<pivot_type>"), pivotType);
    auto timePair = UtilityPivot::fromTimestamp(timestampMs);
    message = std::regex_replace(message, std::regex("<timestamp_sec>"), std::to_string(timePair.first));
    message = std::regex_replace(message, std::regex("<timestamp_sub_sec>"), std::to_string(timePair.second));
    std::string value;
    if (pivotType == ConstantsSystem::JsonCdcSps) {
        value = on?"1":"0";
    }
    else if (pivotType == ConstantsSystem::JsonCdcDps) {
        value = on?QUOTE("on"):QUOTE("off");
    }
    else {
        UtilityPivot::log_fatal("%s Invalid pivot type: %s, message not sent", beforeLog.c_str(), pivotType.c_str());
        return "";
    }
    message = std::regex_replace(message, std::regex("<value>"), value);
    return message;
}

/**
 * Calls the ingest function with the given reading.
 * 
 * @param reading Reading to send through ingest
 */
void NotifySystemSp::ingest(Reading &reading) {
    std::lock_guard<std::mutex> guard(m_ingestMutex);
    std::string beforeLog = ConstantsSystem::NamePlugin + " - NotifySystemSp::ingest : ";
    if (m_ingest == nullptr) {
        UtilityPivot::log_error("%s Callback is not defined", beforeLog.c_str());
        return;
    }
    (*m_ingest)(m_data, &reading);
}

/**
 * Stores the ingest callback function and its data
 *
 * @param ingest  Callback function to cal to ingest
 * @param data  Data required as first parameter of the callbak function
 */
void NotifySystemSp::registerIngest(FuncPtr ingest, void *data) {
    std::lock_guard<std::mutex> guard(m_ingestMutex);
    m_ingest = ingest;
    m_data = data;
}

/**
 * Trigger actions based on the notification received
 *
 * @param notificationName The name of this notification
 * @param triggerReason	Why the notification is being sent
 * @param message The notification message
 * @return True if the notification was process sucessfully, else false
 */
bool NotifySystemSp::notify(const std::string& notificationName, const std::string& triggerReason,
                            const std::string& message) {
    std::lock_guard<std::mutex> guard(m_configMutex);
    std::string beforeLog = ConstantsSystem::NamePlugin + " - NotifySystemSp::notify -";
    if (!isEnabled()) {
        return false;
    }

    // Parse the JSON that represents the reason data
    rapidjson::Document doc;
    doc.Parse(triggerReason.c_str());
    if(doc.HasParseError()) {
        UtilityPivot::log_error("%s Invalid JSON: %s", beforeLog.c_str(), triggerReason.c_str());
        return false;
    }

    if(!doc.HasMember("asset")) {
        UtilityPivot::log_debug("%s Received notification with no 'asset' attribute, ignoring: %s", beforeLog.c_str(), triggerReason.c_str());
        return false;
    }

    if(!doc["asset"].IsString()) {
        UtilityPivot::log_debug("%s Received notification with unknown 'asset' type, ignoring: %s", beforeLog.c_str(), triggerReason.c_str());
        return false;
    }

    std::string asset = doc["asset"].GetString();
    if(asset != "prt.inf") {
        UtilityPivot::log_debug("%s Received notification with unhandled 'asset' value, ignoring: %s", beforeLog.c_str(), triggerReason.c_str());
        return false;
    }

    if(!doc.HasMember("reason")) {
        UtilityPivot::log_error("%s Received notification with no 'reason' attribute, ignoring: %s", beforeLog.c_str(), triggerReason.c_str());
        return false;
    }

    if(!doc["reason"].IsString()) {
        UtilityPivot::log_error("%s Received notification with unknown 'reason' type, ignoring: %s", beforeLog.c_str(), triggerReason.c_str());
        return false;
    }

    std::string reason = doc["reason"].GetString();
    bool connected = false;
    if (reason == "connected") {
        connected = true;
    }
    else if (reason == "connection lost") {
        connected = false;
    }
    else {
        UtilityPivot::log_error("%s Received notification with unhandled 'reason' value, ignoring: %s", beforeLog.c_str(), triggerReason.c_str());
        return false;
    }

    sendConnectionLossSP(connected);
    return true;
}

/**
 * Function sending all the Connection Loss Status Point Readings
 * 
 * @param connected True if the status to send represent a connection fully established, false for a connection loss
 */
void NotifySystemSp::sendConnectionLossSP(bool connected) {
    std::string messageTemplate = getMessageTemplate("prt.inf");
    long currentTimeMs = UtilityPivot::getCurrentTimestampMs();
    const auto& dataSystem = m_configPlugin.getDataSystem();
    for(const auto& dataInfo : dataSystem.at("prt.inf")) {
        std::string jsonReading = fillTemplate(messageTemplate, dataInfo->pivotId, dataInfo->pivotType,
                                               currentTimeMs, connected);
        if (jsonReading.size() == 0) {
            return;
        }
        // Send a reading with data from the template
        sendReading(dataInfo->assetName, jsonReading);
    }
}

/**
 * Reconfiguration entry point to the filter.
 *
 * This method runs holding the configMutex to prevent
 * ingest using the regex class that may be destroyed by this
 * call.
 *
 * Pass the configuration to the base FilterPlugin class and
 * then call the private method to handle the filter specific
 * configuration.
 *
 * @param newConfig  The JSON of the new configuration
 */
void NotifySystemSp::reconfigure(const ConfigCategory& config) {
    std::lock_guard<std::mutex> guard(m_configMutex);
    if (config.itemExists("enable")) {
        m_enabled = config.getValue("enable").compare("true") == 0 ||
                    config.getValue("enable").compare("True") == 0;
    }
    if (config.itemExists("exchanged_data")) {
        setJsonConfig(config.getValue("exchanged_data"));
    }
}
