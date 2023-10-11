#ifndef INCLUDE_CONSTANTS_SYSTEM_H_
#define INCLUDE_CONSTANTS_SYSTEM_H_

#include <string>

#define FILTER_NAME "systemspn"

namespace systemspn {
    
namespace ConstantsSystem {

    static const std::string NamePlugin            = FILTER_NAME;

    constexpr const char *JsonExchangedData           = "exchanged_data";
    constexpr const char *JsonDatapoints              = "datapoints";
    constexpr const char *JsonLabel                   = "label";
    constexpr const char *JsonPivotType               = "pivot_type";
    constexpr const char *JsonPivotId                 = "pivot_id";
    constexpr const char *JsonPivotSubtypes           = "pivot_subtypes";
    constexpr const char *JsonTsSystCycle             = "ts_syst_cycle";

    static const std::string JsonCdcSps     = "SpsTyp";
    static const std::string JsonCdcDps     = "DpsTyp";

    static const std::string KeyMessagePivotJsonRoot       = "PIVOT";
    static const std::string KeyMessagePivotJsonGt         = "GTIS";
    static const std::string KeyMessagePivotJsonId         = "Identifier";
    static const std::string KeyMessagePivotJsonStVal      = "stVal";
    static const std::string KeyMessagePivotJsonT          = "t";
    static const std::string KeyMessagePivotJsonSecondSinceEpoch = "SecondSinceEpoch";
    static const std::string KeyMessagePivotJsonFractSec   = "FractionOfSecond";
    static const std::string KeyMessagePivotJsonQ          = "q";
    static const std::string KeyMessagePivotJsonSource     = "Source";
    static const std::string ValueSubstituted              = "substituted";
    static const std::string KeyMessagePivotJsonTmOrg      = "TmOrg";
};
};

#endif //INCLUDE_CONSTANTS_SYSTEM_H_