#pragma once

#include <array>
#include <cpuinfo.h>
#include <nlohmann/json.hpp>

namespace sigmax {

struct CpuInfo
{
public:

    void QueryCpuInfo();
    nlohmann::json ToJson() const;
private:
    cpuinfo_core coreInfo;
    cpuinfo_package packageInfo;
    cpuinfo_cache l1iCache;
    cpuinfo_cache l1dCache;
    cpuinfo_cache l2Cache;
    cpuinfo_cache l3Cache;
    std::size_t pageSize;

    bool intialized{false};
    static nlohmann::json CacheToJson(const cpuinfo_cache &cache);
    static std::string VendorToString(cpuinfo_vendor vendor);
    static std::string UarchToString(cpuinfo_uarch uarch);
};

CpuInfo GetCpuInfo();


}// namespace sigmax