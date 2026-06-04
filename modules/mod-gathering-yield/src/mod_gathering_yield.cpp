#include "ScriptMgr.h"
#include "Config.h"
#include "Log.h"
#include "LootMgr.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "Random.h"
#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace
{
    bool  g_enable = true;
    float g_multiplier = 2.0f;

    // The six rate-enabled GATHERING loot stores (LootMgr.cpp:44-56). gameobject
    // covers mining veins + herb nodes (+ chests — accepted, no subclass filter).
    bool IsGatheringStore(char const* name)
    {
        static const std::set<std::string> kStores = {
            "gameobject_loot_template",
            "skinning_loot_template",
            "fishing_loot_template",
            "disenchant_loot_template",
            "milling_loot_template",
            "prospecting_loot_template"
        };
        return name && kStores.count(name) != 0;
    }

    // floor(count*M) guaranteed + a frac chance of one more (M >= 1.0).
    uint32 ScaledCount(uint32 baseCount)
    {
        float scaled = static_cast<float>(baseCount) * g_multiplier;
        uint32 guaranteed = static_cast<uint32>(scaled);
        float remainder = scaled - static_cast<float>(guaranteed);   // [0, 1)
        if (remainder > 0.0f && roll_chance_f(remainder * 100.0f))
            ++guaranteed;
        return guaranteed;
    }
}

// --- Config: load on boot + .reload config -------------------------------------
class gathering_yield_world : public WorldScript
{
public:
    gathering_yield_world() : WorldScript("gathering_yield_world") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        g_enable = sConfigMgr->GetOption<bool>("GatheringYield.Enable", true);
        g_multiplier = sConfigMgr->GetOption<float>("GatheringYield.Multiplier", 2.0f);
        if (g_multiplier < 1.0f)
            g_multiplier = 1.0f;
        LOG_INFO("server.loading", "[GatheringYield] enabled={} multiplier={}", g_enable, g_multiplier);
    }
};

// --- Loot: multiply gathering item counts after a fill -------------------------
class gathering_yield_loot : public MiscScript
{
public:
    gathering_yield_loot() : MiscScript("gathering_yield_loot") { }

    void OnAfterLootTemplateProcess(Loot* loot, LootTemplate const* /*tab*/, LootStore const& store,
                                    Player* /*lootOwner*/, bool /*personal*/, bool /*noEmptyError*/,
                                    uint16 /*lootMode*/) override
    {
        if (!g_enable || g_multiplier <= 1.0f || !loot)
            return;
        if (!IsGatheringStore(store.GetName()))
            return;

        // loot->items holds only non-quest items (quest drops live in loot->quest_items,
        // which we never touch). Multiply each in place; collect any stack-overflow into
        // `extras` and append AFTER the loop (push_back would invalidate the `li` ref).
        std::vector<LootItem> extras;
        size_t const originalSize = loot->items.size();
        for (size_t i = 0; i < originalSize; ++i)
        {
            LootItem& li = loot->items[i];

            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(li.itemid);
            if (!proto)
                continue;

            uint32 const baseCount = li.count;
            uint32 const newTotal = ScaledCount(baseCount);
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

void AddGatheringYieldScripts()
{
    new gathering_yield_world();
    new gathering_yield_loot();
}
