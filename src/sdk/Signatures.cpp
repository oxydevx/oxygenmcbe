#include "Signatures.hpp"

#include <libhat.hpp>
#include <Windows.h>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../utils/logger.hpp"

namespace Signatures {

    namespace {
        std::mutex& GetMutex() {
            static std::mutex mtx;
            return mtx;
        }

        std::unordered_map<std::string, uintptr_t>& GetCache() {
            static std::unordered_map<std::string, uintptr_t> cache;
            return cache;
        }

        std::unordered_map<std::string, std::vector<uintptr_t>>& GetCacheAll() {
            static std::unordered_map<std::string, std::vector<uintptr_t>> cacheAll;
            return cacheAll;
        }
    } 

    void Init() {
        int ok = 0;
        int failed = 0;
        std::vector<std::pair<std::string, std::string>> failures;

        for (const auto& def : g_signatures) {
            auto parsed = hat::parse_signature(def.pattern);
            if (!parsed.has_value()) {
                failed++;
                failures.emplace_back(def.name, def.pattern);
                continue;
            }

            
            
            const bool multiple = (std::string(def.name) == "NameProtect");

            if (multiple) {
                auto mod = hat::process::get_process_module();
                auto data = mod.get_section_data(".text");
                auto results = hat::find_all_pattern(data, parsed.value());

                if (results.empty()) {
                    failed++;
                    failures.emplace_back(def.name, def.pattern);
                    continue;
                }

                std::vector<uintptr_t> addrs;
                addrs.reserve(results.size());
                for (auto& r : results)
                    addrs.push_back(reinterpret_cast<uintptr_t>(r.get()));

                std::lock_guard<std::mutex> lock(GetMutex());
                GetCacheAll()[def.name] = std::move(addrs);
                GetCache()[def.name] = GetCacheAll()[def.name][0];
            }
            else {
                auto res = hat::find_pattern(parsed.value(), ".text");
                if (!res.has_result()) {
                    failed++;
                    failures.emplace_back(def.name, def.pattern);
                    continue;
                }

                std::lock_guard<std::mutex> lock(GetMutex());
                GetCache()[def.name] = reinterpret_cast<uintptr_t>(res.get());
            }

            ok++;
        }

        Logger::InfoTag("Signatures", "%d OK, %d failed", ok, failed);

        if (!failures.empty()) {
            Logger::ErrorTag("Signatures", "Failures:");
            for (auto& [name, pattern] : failures)
                Logger::ErrorTag("Signatures", "%s :: %s", name.c_str(), pattern.c_str());
        }
    }

    uintptr_t Get(const std::string& name) {
        std::lock_guard<std::mutex> lock(GetMutex());
        auto it = GetCache().find(name);
        return it != GetCache().end() ? it->second : 0;
    }

    const std::vector<uintptr_t>& GetAll(const std::string& name) {
        std::lock_guard<std::mutex> lock(GetMutex());
        static const std::vector<uintptr_t> empty;
        auto it = GetCacheAll().find(name);
        return it != GetCacheAll().end() ? it->second : empty;
    }

} 
