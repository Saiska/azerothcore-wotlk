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
    // Bare "class=mult" rules.
    std::unordered_map<uint32, float> g_classRates;
    // "class:subclass=mult" rules, keyed by (class << 8) | (subclass & 0xFF).
    std::unordered_map<uint32, float> g_subRates;

    inline uint32 SubKey(uint32 cls, uint32 sub) { return (cls << 8) | (sub & 0xFFu); }

    // Sub-specific rule wins over the class-wide rule; else no scaling (1.0).
    float LookupMult(uint32 cls, uint32 sub)
    {
        auto si = g_subRates.find(SubKey(cls, sub));
        if (si != g_subRates.end())
            return si->second;
        auto ci = g_classRates.find(cls);
        if (ci != g_classRates.end())
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

    // Parse "3=2.0 7=2.0 7:9=3.0" into g_classRates / g_subRates. Returns a compact
    // summary string for the boot log. Rules with mult <= 1.0 are dropped.
    std::string ParseRates(std::string const& raw)
    {
        g_classRates.clear();
        g_subRates.clear();
        std::string summary;
        std::istringstream iss(raw);
        std::string tok;
        while (iss >> tok)
        {
            std::string::size_type eq = tok.find('=');
            if (eq == std::string::npos)
                continue;
            float mult = static_cast<float>(std::atof(tok.substr(eq + 1).c_str()));
            if (mult <= 1.0f)
                continue;
            std::string key = tok.substr(0, eq);
            std::string::size_type colon = key.find(':');
            if (colon == std::string::npos)
            {
                uint32 cls = static_cast<uint32>(std::atoi(key.c_str()));
                g_classRates[cls] = mult;
            }
            else
            {
                uint32 cls = static_cast<uint32>(std::atoi(key.substr(0, colon).c_str()));
                uint32 sub = static_cast<uint32>(std::atoi(key.substr(colon + 1).c_str()));
                g_subRates[SubKey(cls, sub)] = mult;
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
        std::string raw = sConfigMgr->GetOption<std::string>("LootMultipliers.CategoryRates", "3=2.0 7=2.0");
        std::string summary = ParseRates(raw);
        LOG_INFO("server.loading", "[LootMultipliers] enabled={} rules={} ({})",
                 g_enable, g_classRates.size() + g_subRates.size(), summary);
    }
};

// --- Loot: multiply item counts by category after a fill -----------------------
class loot_multipliers_loot : public MiscScript
{
public:
    loot_multipliers_loot() : MiscScript("loot_multipliers_loot") { }

    void OnAfterLootTemplateProcess(Loot* loot, LootTemplate const* /*tab*/, LootStore const& /*store*/,
                                    Player* /*lootOwner*/, bool /*personal*/, bool /*noEmptyError*/,
                                    uint16 /*lootMode*/) override
    {
        if (!g_enable || !loot || (g_classRates.empty() && g_subRates.empty()))
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

            float const m = LookupMult(proto->Class, proto->SubClass);
            if (m <= 1.0f)
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
