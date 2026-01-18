#include "benchmark_utils.hpp"

#include "log.hpp"
#include <cstdint>

namespace sigmax {

CpuInfo GetCpuInfo()
{
    cpuinfo_initialize();
    CpuInfo cpuInfo{};
    cpuInfo.coreInfo = *cpuinfo_get_core(0);
    cpuInfo.packageInfo = *cpuinfo_get_package(0);
    // assuming the caches are identical for all cores
    cpuInfo.l1iCache = *cpuinfo_get_l1i_cache(0);
    cpuInfo.l1dCache = *cpuinfo_get_l1d_cache(0);
    cpuInfo.l2Cache = *cpuinfo_get_l2_cache(0);
    cpuInfo.l3Cache = *cpuinfo_get_l3_cache(0);
    cpuInfo.intialized = true;
    return cpuInfo;
}

nlohmann::json CpuInfo::ToJson() const
{
    nlohmann::json json;
    json["vendor"] = VendorToString(coreInfo.vendor);
    json["uarch"] = UarchToString(coreInfo.uarch);
    json["l1iCache"] = CacheToJson(l1iCache);
    json["l1dCache"] = CacheToJson(l1dCache);
    json["l2Cache"] = CacheToJson(l2Cache);
    json["l3Cache"] = CacheToJson(l3Cache);
    json["coresPerSocket"] = packageInfo.core_count;
    return json;
}

nlohmann::json CpuInfo::CacheToJson(const cpuinfo_cache &cache)
{
    nlohmann::json json;
    json["size"] = cache.size;
    json["associativity"] = cache.associativity;
    json["line_size"] = cache.line_size;
    return json;
}

std::string CpuInfo::VendorToString(cpuinfo_vendor vendor)
{
    // not decoding all the supported vendors, only the ones we have a chance of using
    switch (vendor) {
        case cpuinfo_vendor_intel:
            return "Intel";
        case cpuinfo_vendor_amd:
            return "AMD";
        case cpuinfo_vendor_arm:
            return "ARM";
        case cpuinfo_vendor_qualcomm:
            return "Qualcomm";
        case cpuinfo_vendor_apple:
            return "Apple";
        case cpuinfo_vendor_nvidia:
            return "NVIDIA";
        case cpuinfo_vendor_mips:
            return "MIPS";
        case cpuinfo_vendor_ibm:
            return "IBM";
        case cpuinfo_vendor_broadcom:
            return "Broadcom";
        case cpuinfo_vendor_marvell:
            return "Marvell";
        default:
            LOG_ERROR("Unknown vendor: {:x}", static_cast<unsigned int>(vendor));
            return "Unknown vendor";
    }
}

std::string CpuInfo::UarchToString(cpuinfo_uarch uarch)
{
    switch (uarch) {
        case cpuinfo_uarch_unknown:
            return "Unknown";

        // Intel P5
        case cpuinfo_uarch_p5:
            return "Pentium and Pentium MMX";
        case cpuinfo_uarch_quark:
            return "Intel Quark";

        // Intel P6
        case cpuinfo_uarch_p6:
            return "Pentium Pro, Pentium II, Pentium III";
        case cpuinfo_uarch_dothan:
            return "Pentium M";
        case cpuinfo_uarch_yonah:
            return "Intel Core";
        case cpuinfo_uarch_conroe:
            return "Intel Core 2 (65 nm)";
        case cpuinfo_uarch_penryn:
            return "Intel Core 2 (45 nm)";
        case cpuinfo_uarch_nehalem:
            return "Intel Nehalem/Westmere (Core i3/i5/i7 1st gen)";
        case cpuinfo_uarch_sandy_bridge:
            return "Intel Sandy Bridge (Core i3/i5/i7 2nd gen)";
        case cpuinfo_uarch_ivy_bridge:
            return "Intel Ivy Bridge (Core i3/i5/i7 3rd gen)";
        case cpuinfo_uarch_haswell:
            return "Intel Haswell (Core i3/i5/i7 4th gen)";
        case cpuinfo_uarch_broadwell:
            return "Intel Broadwell";
        case cpuinfo_uarch_sky_lake:
            return "Intel Sky Lake (14 nm)";
        //case cpuinfo_uarch_kaby_lake:
        //    return "Intel Kaby Lake";
        case cpuinfo_uarch_palm_cove:
            return "Intel Palm Cove (Cannon Lake)";
        case cpuinfo_uarch_sunny_cove:
            return "Intel Sunny Cove (Ice Lake)";
        case cpuinfo_uarch_willow_cove:
            return "Intel Willow Cove (Tiger Lake)";

        // Intel Pentium 4
        case cpuinfo_uarch_willamette:
            return "Pentium 4 (Willamette/Northwood/Foster)";
        case cpuinfo_uarch_prescott:
            return "Pentium 4 (Prescott)";

        // Intel Atom
        case cpuinfo_uarch_bonnell:
            return "Intel Atom (45 nm)";
        case cpuinfo_uarch_saltwell:
            return "Intel Atom (32 nm)";
        case cpuinfo_uarch_silvermont:
            return "Intel Silvermont (22 nm)";
        case cpuinfo_uarch_airmont:
            return "Intel Airmont (14 nm)";
        case cpuinfo_uarch_goldmont:
            return "Intel Goldmont";
        case cpuinfo_uarch_goldmont_plus:
            return "Intel Goldmont Plus";
        case cpuinfo_uarch_tremont:
            return "Intel Tremont (10 nm)";
        case cpuinfo_uarch_gracemont:
            return "Intel Gracemont (AlderLake N)";
        case cpuinfo_uarch_crestmont:
            return "Intel Crestmont (Sierra Forest)";
        case cpuinfo_uarch_darkmont:
            return "Intel Darkmont (Clearwater Forest)";

        // Intel Knights/Xeon Phi
        case cpuinfo_uarch_knights_ferry:
            return "Intel Knights Ferry";
        case cpuinfo_uarch_knights_corner:
            return "Intel Knights Corner (Xeon Phi)";
        case cpuinfo_uarch_knights_landing:
            return "Intel Knights Landing";
        case cpuinfo_uarch_knights_hill:
            return "Intel Knights Hill";
        case cpuinfo_uarch_knights_mill:
            return "Intel Knights Mill";

        // Intel/Marvell XScale
        case cpuinfo_uarch_xscale:
            return "Intel/Marvell XScale";

        // AMD K series
        case cpuinfo_uarch_k5:
            return "AMD K5";
        case cpuinfo_uarch_k6:
            return "AMD K6";
        case cpuinfo_uarch_k7:
            return "AMD Athlon/Duron (K7)";
        case cpuinfo_uarch_k8:
            return "AMD Athlon 64/Opteron 64 (K8)";
        case cpuinfo_uarch_k10:
            return "AMD Family 10h (K10)";
        case cpuinfo_uarch_bulldozer:
            return "AMD Bulldozer";
        case cpuinfo_uarch_piledriver:
            return "AMD Piledriver";
        case cpuinfo_uarch_steamroller:
            return "AMD Steamroller";
        case cpuinfo_uarch_excavator:
            return "AMD Excavator";
        case cpuinfo_uarch_zen:
            return "AMD Zen";
        case cpuinfo_uarch_zen2:
            return "AMD Zen 2";
        case cpuinfo_uarch_zen3:
            return "AMD Zen 3";
        case cpuinfo_uarch_zen4:
            return "AMD Zen 4";
        case cpuinfo_uarch_zen5:
            return "AMD Zen 5";

        // AMD Geode and mobile
        case cpuinfo_uarch_geode:
            return "NSC/AMD Geode";
        case cpuinfo_uarch_bobcat:
            return "AMD Bobcat";
        case cpuinfo_uarch_jaguar:
            return "AMD Jaguar";
        case cpuinfo_uarch_puma:
            return "AMD Puma";

        // ARM classic
        case cpuinfo_uarch_arm7:
            return "ARM7";
        case cpuinfo_uarch_arm9:
            return "ARM9";
        case cpuinfo_uarch_arm11:
            return "ARM11";

        // ARM Cortex-A (32-bit)
        case cpuinfo_uarch_cortex_a5:
            return "ARM Cortex-A5";
        case cpuinfo_uarch_cortex_a7:
            return "ARM Cortex-A7";
        case cpuinfo_uarch_cortex_a8:
            return "ARM Cortex-A8";
        case cpuinfo_uarch_cortex_a9:
            return "ARM Cortex-A9";
        case cpuinfo_uarch_cortex_a12:
            return "ARM Cortex-A12";
        case cpuinfo_uarch_cortex_a15:
            return "ARM Cortex-A15";
        case cpuinfo_uarch_cortex_a17:
            return "ARM Cortex-A17";

        // ARM Cortex-A (64-bit)
        case cpuinfo_uarch_cortex_a32:
            return "ARM Cortex-A32";
        case cpuinfo_uarch_cortex_a35:
            return "ARM Cortex-A35";
        case cpuinfo_uarch_cortex_a53:
            return "ARM Cortex-A53";
        case cpuinfo_uarch_cortex_a55r0:
            return "ARM Cortex-A55 (r0)";
        case cpuinfo_uarch_cortex_a55:
            return "ARM Cortex-A55";
        case cpuinfo_uarch_cortex_a57:
            return "ARM Cortex-A57";
        case cpuinfo_uarch_cortex_a65:
            return "ARM Cortex-A65";
        case cpuinfo_uarch_cortex_a72:
            return "ARM Cortex-A72";
        case cpuinfo_uarch_cortex_a73:
            return "ARM Cortex-A73";
        case cpuinfo_uarch_cortex_a75:
            return "ARM Cortex-A75";
        case cpuinfo_uarch_cortex_a76:
            return "ARM Cortex-A76";
        case cpuinfo_uarch_cortex_a77:
            return "ARM Cortex-A77";
        case cpuinfo_uarch_cortex_a78:
            return "ARM Cortex-A78";

        // ARM Neoverse
        case cpuinfo_uarch_neoverse_n1:
            return "ARM Neoverse N1";
        case cpuinfo_uarch_neoverse_e1:
            return "ARM Neoverse E1";
        case cpuinfo_uarch_neoverse_v1:
            return "ARM Neoverse V1";
        case cpuinfo_uarch_neoverse_n2:
            return "ARM Neoverse N2";
        case cpuinfo_uarch_neoverse_v2:
            return "ARM Neoverse V2";

        // ARM Cortex-X
        case cpuinfo_uarch_cortex_x1:
            return "ARM Cortex-X1";
        case cpuinfo_uarch_cortex_x2:
            return "ARM Cortex-X2";
        case cpuinfo_uarch_cortex_x3:
            return "ARM Cortex-X3";
        case cpuinfo_uarch_cortex_x4:
            return "ARM Cortex-X4";
        case cpuinfo_uarch_cortex_x925:
            return "ARM Cortex-X925";

        // ARM Cortex-A (newer)
        case cpuinfo_uarch_cortex_a510:
            return "ARM Cortex-A510";
        case cpuinfo_uarch_cortex_a520:
            return "ARM Cortex-A520";
        case cpuinfo_uarch_cortex_a710:
            return "ARM Cortex-A710";
        case cpuinfo_uarch_cortex_a715:
            return "ARM Cortex-A715";
        case cpuinfo_uarch_cortex_a720:
            return "ARM Cortex-A720";
        case cpuinfo_uarch_cortex_a725:
            return "ARM Cortex-A725";

        // Qualcomm
        case cpuinfo_uarch_scorpion:
            return "Qualcomm Scorpion";
        case cpuinfo_uarch_krait:
            return "Qualcomm Krait";
        case cpuinfo_uarch_kryo:
            return "Qualcomm Kryo";
        case cpuinfo_uarch_falkor:
            return "Qualcomm Falkor";
        case cpuinfo_uarch_saphira:
            return "Qualcomm Saphira";
        case cpuinfo_uarch_oryon:
            return "Qualcomm Oryon";
        case cpuinfo_uarch_oryon_v3:
            return "Qualcomm Oryon V3";

        // NVIDIA
        case cpuinfo_uarch_denver:
            return "NVIDIA Denver";
        case cpuinfo_uarch_denver2:
            return "NVIDIA Denver 2";
        case cpuinfo_uarch_carmel:
            return "NVIDIA Carmel";

        // Samsung Exynos
        case cpuinfo_uarch_exynos_m1:
            return "Samsung Exynos M1";
        case cpuinfo_uarch_exynos_m2:
            return "Samsung Exynos M2";
        case cpuinfo_uarch_exynos_m3:
            return "Samsung Exynos M3";
        case cpuinfo_uarch_exynos_m4:
            return "Samsung Exynos M4";
        case cpuinfo_uarch_exynos_m5:
            return "Samsung Exynos M5";

        // Apple
        case cpuinfo_uarch_swift:
            return "Apple Swift (A6/A6X)";
        case cpuinfo_uarch_cyclone:
            return "Apple Cyclone (A7)";
        case cpuinfo_uarch_typhoon:
            return "Apple Typhoon (A8/A8X)";
        case cpuinfo_uarch_twister:
            return "Apple Twister (A9/A9X)";
        case cpuinfo_uarch_hurricane:
            return "Apple Hurricane (A10/A10X)";
        case cpuinfo_uarch_monsoon:
            return "Apple Monsoon (A11 big)";
        case cpuinfo_uarch_mistral:
            return "Apple Mistral (A11 little)";
        case cpuinfo_uarch_vortex:
            return "Apple Vortex (A12 big)";
        case cpuinfo_uarch_tempest:
            return "Apple Tempest (A12 little)";
        case cpuinfo_uarch_lightning:
            return "Apple Lightning (A13 big)";
        case cpuinfo_uarch_thunder:
            return "Apple Thunder (A13 little)";
        case cpuinfo_uarch_firestorm:
            return "Apple Firestorm (A14/M1 big)";
        case cpuinfo_uarch_icestorm:
            return "Apple Icestorm (A14/M1 little)";
        case cpuinfo_uarch_avalanche:
            return "Apple Avalanche (A15/M2 big)";
        case cpuinfo_uarch_blizzard:
            return "Apple Blizzard (A15/M2 little)";
        case cpuinfo_uarch_everest:
            return "Apple Everest (A16 big)";
        case cpuinfo_uarch_sawtooth:
            return "Apple Sawtooth (A16 little)";
        case cpuinfo_uarch_coll_everest:
            return "Apple Coll Everest (A17 big)";
        case cpuinfo_uarch_coll_sawtooth:
            return "Apple Coll Sawtooth (A17 little)";
        case cpuinfo_uarch_tupai_everest:
            return "Apple Tupai Everest (A18 big)";
        case cpuinfo_uarch_tupai_sawtooth:
            return "Apple Tupai Sawtooth (A18 little)";
        case cpuinfo_uarch_tahiti_everest:
            return "Apple Tahiti Everest (A18 Pro big)";
        case cpuinfo_uarch_tahiti_sawtooth:
            return "Apple Tahiti Sawtooth (A18 Pro little)";
        case cpuinfo_uarch_tilos_everest:
            return "Apple Tilos Everest (A19 big)";
        case cpuinfo_uarch_tilos_sawtooth:
            return "Apple Tilos Sawtooth (A19 little)";
        case cpuinfo_uarch_donan_everest:
            return "Apple Donan Everest (M4 big)";
        case cpuinfo_uarch_donan_sawtooth:
            return "Apple Donan Sawtooth (M4 little)";

        // Cavium
        case cpuinfo_uarch_thunderx:
            return "Cavium ThunderX";
        case cpuinfo_uarch_thunderx2:
            return "Cavium ThunderX2";

        // Marvell
        case cpuinfo_uarch_pj4:
            return "Marvell PJ4";

        // Broadcom
        case cpuinfo_uarch_brahma_b15:
            return "Broadcom Brahma B15";
        case cpuinfo_uarch_brahma_b53:
            return "Broadcom Brahma B53";

        // Applied Micro
        case cpuinfo_uarch_xgene:
            return "Applied Micro X-Gene";

        // Hygon
        case cpuinfo_uarch_dhyana:
            return "Hygon Dhyana";

        // HiSilicon
        case cpuinfo_uarch_taishan_v110:
            return "HiSilicon TaiShan v110";

        default:
            LOG_ERROR("Unknown microarchitecture: {:x}", static_cast<int>(uarch));
            return "Unknown";
    }
}

}// namespace sigmax