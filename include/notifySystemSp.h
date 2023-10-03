#ifndef INCLUDE_FILTER_SYSTEM_SP_H_
#define INCLUDE_FILTER_SYSTEM_SP_H_

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

#include <datapoint_utility.h>
#include <config_category.h>
#include <mutex>
#include <string>
#include <thread>
#include <atomic>

#include "configPlugin.h"

using FuncPtr = void (*)(void *, void *);

namespace systemspn {

class NotifySystemSp {
public:  
    NotifySystemSp() = default;
    NotifySystemSp& operator=(const NotifySystemSp& other) = delete;
    NotifySystemSp(NotifySystemSp &&other) noexcept = delete;
    NotifySystemSp const & operator=(NotifySystemSp &&other) = delete;
    ~NotifySystemSp();

    void reconfigure(const ConfigCategory& config);
    void setJsonConfig(const std::string& jsonExchanged);
    const ConfigPlugin& getConfigPlugin() const { return m_configPlugin; }
    bool isEnabled() const { return m_enabled; } 

    void registerIngest(FuncPtr ingest, void *data);
    void ingest(Reading &reading);

    std::string getMessageTemplate(const std::string& dataType) const;
    void startCycles();
    void stopCycles();
    void runCycles(const std::string& messageTemplate, const std::string& pivotId,
                   const std::string& pivotType, const std::string& assetName, int cycleSec);
    std::string fillTemplate(const std::string& messageTemplate, const std::string& pivotId,
                             const std::string& pivotType, long timestampMs, bool on = true) const;
    void sendReading(const std::string& assetName, const std::string& jsonReading);
    bool notify(const std::string& notificationName, const std::string& triggerReason, const std::string& message);
    void sendConnectionLossSP(bool connected);

private:
    void*	                 m_data = nullptr;
    FuncPtr	                 m_ingest = nullptr;
    mutable std::mutex       m_ingestMutex;
    ConfigPlugin             m_configPlugin;
    mutable std::mutex       m_configMutex;
    std::vector<std::thread> m_cycleThreads;
    std::atomic<bool>        m_isRunning{false};
    std::atomic<bool>        m_enabled{false};
};
};

#endif  // INCLUDE_FILTER_SYSTEM_SP_H_