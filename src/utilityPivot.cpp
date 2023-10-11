/*
 * Utility.
 *
 * Copyright (c) 2020, RTE (https://www.rte-france.com)
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Yannick Marchetaux
 * 
 */
#include <cmath>
#include <chrono>
#include <sstream>

#include "utilityPivot.h"

using namespace systemspn;

/**
 * Convert secondSinceEpoch and secondSinceEpoch to timestamp
 * @param secondSinceEpoch : interval in seconds continuously counted from the epoch 1970-01-01 00:00:00 UTC
 * @param fractionOfSecond : represents the fraction of the current second when the value of the TimeStamp has been determined.
 * @return timestamp (ms)
*/
long UtilityPivot::toTimestamp(long secondSinceEpoch, long fractionOfSecond) {
    long timestamp = 0;
    auto msPart = (long)round((double)(fractionOfSecond * 1000) / 16777216.0);
    timestamp = (secondSinceEpoch * 1000L) + msPart;
    return timestamp;
}

/**
 * Convert timestamp (ms) in pair of secondSinceEpoch and fractionOfSecond
 * @param timestamp : timestamp (ms) 
 * @return pair of secondSinceEpoch and fractionOfSecond
*/
std::pair<long, long> UtilityPivot::fromTimestamp(long timestamp) {
    long remainder = (timestamp % 1000L);
    long fractionOfSecond = remainder * 16777 + ((remainder * 216) / 1000);
    return std::make_pair(timestamp / 1000L, fractionOfSecond);
}

/**
 * Get current timestamp in milisecond
 * @return timestamp in ms
*/
long UtilityPivot::getCurrentTimestampMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

/**
 * Join a list of strings into a single string with the given separator
 * @param list : List of strings to join
 * @param sep : Separator to put bewteen each list element
 * @return String containing the concatenation of all strings in the list with separator inbetween
*/
std::string UtilityPivot::join(const std::vector<std::string> &list, const std::string &sep /*= ", "*/)
{
    std::string ret;
    for(const auto &str : list) {
        if(!ret.empty()) {
            ret += sep;
        }
        ret += str;
    }
    return ret;
}

/**
 * Split a string into a list of strings with the given separator
 * @param str : List of strings to join
 * @param sep : Separator split each list element
 * @return List of strings extracted from the initial string
*/
std::vector<std::string> UtilityPivot::split(const std::string& str, char sep) {
    std::stringstream ss(str);
    std::string item;
    std::vector<std::string> elems;
    while (std::getline(ss, item, sep)) {
        elems.push_back(std::move(item));
    }
    if (elems.empty()) {
        elems.push_back(str);
    }
    return elems;
}
