#ifndef INCLUDE_CONFIG_PLUGIN_H_
#define INCLUDE_CONFIG_PLUGIN_H_

/*
 * Import confiugration Exchanged data
 *
 * Copyright (c) 2020, RTE (https://www.rte-france.com)
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Yannick Marchetaux
 * 
 */
#include <vector>
#include <string>
#include <memory>
#include <map>

#include <rapidjson/document.h>

namespace systemspn {

// Generic data info struct
struct DataInfo {
    std::string pivotId;
    std::string pivotType;
    std::string assetName;

    DataInfo(const std::string& pivotIdInit, const std::string& pivotTypeInit, const std::string& assetNameInit):
        pivotId(pivotIdInit), pivotType(pivotTypeInit), assetName(assetNameInit) {}
    virtual ~DataInfo() = default; // Needed for dynamic_cast
};

// Data info specific to cyclic Status Points
struct CyclicDataInfo : public DataInfo {
    int cycleSec = 0; // Status point emission cycle in seconds

    CyclicDataInfo(const std::string& pivotIdInit, const std::string& pivotTypeInit,
                   const std::string& assetNameInit, int cycleSecInit):
        DataInfo{pivotIdInit, pivotTypeInit, assetNameInit}, cycleSec(cycleSecInit) {}
    ~CyclicDataInfo() override = default; // Needed for dynamic_cast
};

class ConfigPlugin {
public:  
    ConfigPlugin();

    void importExchangedData(const std::string & exchangeConfig);
    bool hasDataForType(const std::string& dataType, const std::string& pivotId) const;
    void addDataInfo(const std::string& dataType, std::shared_ptr<DataInfo> dataInfo);

    const std::map<std::string, std::vector<std::shared_ptr<DataInfo>>>& getDataSystem() const { return m_dataSystem; }
    const std::vector<std::string>& getDataTypes() const { return m_allDataTypes; }
    
private:
    void m_reset();
    void m_importDatapoint(const rapidjson::Value& datapoint);

    std::vector<std::string> m_allDataTypes{"acces", "prt.inf"};
    std::map<std::string, std::vector<std::shared_ptr<DataInfo>>> m_dataSystem;
};

};

#endif  // INCLUDE_CONFIG_PLUGIN_H_