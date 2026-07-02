#include "ScriptMgr.h"
#include "Config.h"
#include "Log.h"
#include "LootMgr.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "Random.h"
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    bool g_enable = true;
    // "class=mult" rules, keyed by item class — separate lower / upper bounds.
    std::unordered_map<uint32, float> g_classMinRates;
    std::unordered_map<uint32, float> g_classMaxRates;
    // "class:subclass=mult" rules, keyed by (class << 8) | (subclass & 0xFF).
    std::unordered_map<uint32, float> g_subMinRates;
    std::unordered_map<uint32, float> g_subMaxRates;

    inline uint32 SubKey(uint32 cls, uint32 sub) { return (cls << 8) | (sub & 0xFFu); }

    // Sub-specific rule wins over the class-wide rule; else no scaling (1.0). Resolved
    // against a caller-supplied (class, sub) map pair so the SAME logic serves both bounds.
    float LookupMult(std::unordered_map<uint32, float> const& classMap,
                     std::unordered_map<uint32, float> const& subMap,
                     uint32 cls, uint32 sub)
    {
        auto si = subMap.find(SubKey(cls, sub));
        if (si != subMap.end())
            return si->second;
        auto ci = classMap.find(cls);
        if (ci != classMap.end())
            return ci->second;
        return 1.0f;
    }

    // floor(count*M) guaranteed + a frac chance of one more (M >= 1.0).
    uint32 ScaledCount(uint32 baseCount, float m)
    {
        float scaled = static_cast<float>(baseCount) * m;
        uint32 guaranteed = static_cast<uint32>(scaled);
        float remainder = scaled - static_cast<float>(guaranteed);   // [0, 1)
        if (remainder > 0.0f && roll_chance_f(remainder * 100.0f))
            ++guaranteed;
        return guaranteed;
    }

    // Parse "3=2.0 7=2.0 7:9=3.0" into the supplied class/sub maps. Returns a compact
    // summary for the boot log. NO <=1.0 filter — a min (or max) of 1.0 is legal now
    // (the <=1.0 guard lives at the per-item roll site).
    std::string ParseRates(std::string const& raw,
                           std::unordered_map<uint32, float>& classMap,
                           std::unordered_map<uint32, float>& subMap)
    {
        classMap.clear();
        subMap.clear();
        std::string summary;
        std::istringstream iss(raw);
        std::string tok;
        while (iss >> tok)
        {
            std::string::size_type eq = tok.find('=');
            if (eq == std::string::npos)
                continue;
            float mult = static_cast<float>(std::atof(tok.substr(eq + 1).c_str()));
            std::string key = tok.substr(0, eq);
            std::string::size_type colon = key.find(':');
            if (colon == std::string::npos)
            {
                uint32 cls = static_cast<uint32>(std::atoi(key.c_str()));
                classMap[cls] = mult;
            }
            else
            {
                uint32 cls = static_cast<uint32>(std::atoi(key.substr(0, colon).c_str()));
                uint32 sub = static_cast<uint32>(std::atoi(key.substr(colon + 1).c_str()));
                subMap[SubKey(cls, sub)] = mult;
            }
            if (!summary.empty())
                summary += ' ';
            summary += tok;
        }
        return summary;
    }
}

// --- Config: load on boot + .reload config -------------------------------------
class loot_multipliers_world : public WorldScript
{
public:
    loot_multipliers_world() : WorldScript("loot_multipliers_world") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        g_enable = sConfigMgr->GetOption<bool>("LootMultipliers.Enable", true);
        std::string rawMin = sConfigMgr->GetOption<std::string>("LootMultipliers.CategoryMinRates", "3=1.0 7=1.0");
        std::string rawMax = sConfigMgr->GetOption<std::string>("LootMultipliers.CategoryMaxRates", "3=2.0 7=2.0");
        std::string minSummary = ParseRates(rawMin, g_classMinRates, g_subMinRates);
        std::string maxSummary = ParseRates(rawMax, g_classMaxRates, g_subMaxRates);
        LOG_INFO("server.loading", "[LootMultipliers] enabled={} rules={} min({}) max({})",
                 g_enable, g_classMaxRates.size() + g_subMaxRates.size(), minSummary, maxSummary);
    }
};

// --- Loot: randomly multiply item counts by category after a fill --------------
class loot_multipliers_loot : public MiscScript
{
public:
    loot_multipliers_loot() : MiscScript("loot_multipliers_loot") { }

    void OnAfterLootTemplateProcess(Loot* loot, LootTemplate const* /*tab*/, LootStore const& /*store*/,
                                    Player* /*lootOwner*/, bool /*personal*/, bool /*noEmptyError*/,
                                    uint16 /*lootMode*/) override
    {
        if (!g_enable || !loot || (g_classMaxRates.empty() && g_subMaxRates.empty()))
            return;

        // loot->items holds only non-quest items (quest drops live in loot->quest_items,
        // which we never touch). Multiply each matching item in place; collect any
        // stack-overflow into `extras` and append AFTER the loop (push_back would
        // invalidate the live `li` reference).
        std::vector<LootItem> extras;
        size_t const originalSize = loot->items.size();
        for (size_t i = 0; i < originalSize; ++i)
        {
            LootItem& li = loot->items[i];

            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(li.itemid);
            if (!proto)
                continue;
            if (proto->Class == ITEM_CLASS_QUEST)        // safety: never inflate quest items
                continue;

            // Roll a fresh uniform multiplier in [min, max] for THIS item's category.
            // min/max resolve independently (each: sub-override else class-wide else 1.0).
            float minM = LookupMult(g_classMinRates, g_subMinRates, proto->Class, proto->SubClass);
            float maxM = LookupMult(g_classMaxRates, g_subMaxRates, proto->Class, proto->SubClass);
            if (maxM < minM)                             // guard inverted / asymmetric config
                maxM = minM;
            float const m = frand(minM, maxM);
            if (m <= 1.0f)                               // loot is never reduced
                continue;

            uint32 const baseCount = li.count;
            uint32 const newTotal = ScaledCount(baseCount, m);
            if (newTotal <= baseCount)
                continue;

            uint32 maxStack = proto->GetMaxStackSize();
            if (maxStack < 1)
                maxStack = 1;

            LootItem const templ = li;   // copy template before any reallocation
            li.count = static_cast<uint8>(std::min<uint32>(newTotal, maxStack));

            uint32 remaining = newTotal - li.count;
            while (remaining > 0)
            {
                LootItem extra = templ;
                uint32 const thisStack = std::min<uint32>(remaining, maxStack);
                extra.count = static_cast<uint8>(thisStack);
                extras.push_back(extra);
                remaining -= thisStack;
            }
        }

        for (LootItem& e : extras)
        {
            if (loot->items.size() >= MAX_NR_LOOT_ITEMS)   // loot-list cap (LootMgr.h)
                break;
            e.itemIndex = loot->items.size();
            loot->items.push_back(e);
        }
    }
};

void AddLootMultipliersScripts()
{
    new loot_multipliers_world();
    new loot_multipliers_loot();
}
