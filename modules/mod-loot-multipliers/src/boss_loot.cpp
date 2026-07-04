#include "ScriptMgr.h"
#include "Config.h"
#include "Log.h"
#include "LootMgr.h"
#include "Creature.h"
#include "GameObject.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "Player.h"
#include "SharedDefines.h"
#include "Random.h"
#include "World.h"

namespace
{
    bool   g_bossEnable   = true;
    uint32 g_bossCountMin = 2;
    uint32 g_bossCountMax = 3;
    bool   g_bossDebug    = false;   // LootMultipliers.BossDebug: verbose per-fill diagnostics

    // True only for loot owned by a dungeon/raid boss: a boss creature's corpse, or a
    // boss-drop chest (a chest that spawned at runtime inside an instance, GetSpawnId()==0).
    // sourceWorldObjectGUID is set at spawn (Creature/GameObject::AddToWorld), so it is always
    // populated by the time a loot hook fires. player is the resolve anchor (same map as the
    // source); a null player (rare fill paths) => not a boss, no boost.
    bool IsBossLoot(Player const* player, Loot const& loot)
    {
        if (!player)
            return false;

        ObjectGuid const guid = loot.sourceWorldObjectGUID;
        if (!guid)
            return false;

        Map* map = player->GetMap();
        if (!map || !map->IsDungeon())          // 5-man instances AND raids; excludes open world
            return false;

        if (guid.IsCreatureOrVehicle())
        {
            if (Creature* creature = ObjectAccessor::GetCreature(*player, guid))
            {
                bool const wb = creature->isWorldBoss();
                bool const db = creature->IsDungeonBoss();
                if (g_bossDebug)
                    LOG_INFO("server.loading",
                        "[BossLoot dbg] creature entry={} name={} worldBoss={} dungeonBoss={} => boss={}",
                        creature->GetEntry(), creature->GetName(), wb, db, (wb || db));
                return wb || db;
            }
            if (g_bossDebug)
                LOG_INFO("server.loading", "[BossLoot dbg] creature guid={} NOT RESOLVED (no boost)",
                    guid.ToString());
        }
        else if (guid.IsGameObject())
        {
            if (GameObject* go = ObjectAccessor::GetGameObject(*player, guid))
            {
                bool const isChest = go->GetGoType() == GAMEOBJECT_TYPE_CHEST && go->GetSpawnId() == 0;
                if (g_bossDebug)
                    LOG_INFO("server.loading",
                        "[BossLoot dbg] gob entry={} goType={} spawnId={} => bossChest={}",
                        go->GetEntry(), uint32(go->GetGoType()), go->GetSpawnId(), isChest);
                return isChest;
            }
        }

        return false;
    }

    // A fresh 2..3 (config) roll, taken per group / per reference of a boss's loot.
    uint32 BossRoll()
    {
        return urand(g_bossCountMin, g_bossCountMax);
    }
}

// --- Config: load on boot + .reload config -------------------------------------
class boss_loot_world : public WorldScript
{
public:
    boss_loot_world() : WorldScript("boss_loot_world") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        g_bossEnable   = sConfigMgr->GetOption<bool>("LootMultipliers.BossEnable", true);
        g_bossCountMin = sConfigMgr->GetOption<uint32>("LootMultipliers.BossCountMin", 2);
        g_bossCountMax = sConfigMgr->GetOption<uint32>("LootMultipliers.BossCountMax", 3);
        g_bossDebug    = sConfigMgr->GetOption<bool>("LootMultipliers.BossDebug", false);

        if (g_bossCountMin < 1)
            g_bossCountMin = 1;
        if (g_bossCountMax < g_bossCountMin)
            g_bossCountMax = g_bossCountMin;

        LOG_INFO("server.loading", "[BossLoot] enabled={} min={} max={} debug={}",
            g_bossEnable, g_bossCountMin, g_bossCountMax, g_bossDebug);
    }
};

// --- Loot: re-roll a boss's groups / references 2..3x (distinct drops) ----------
class boss_loot_global : public GlobalScript
{
public:
    boss_loot_global() : GlobalScript("boss_loot_global") { }

    // 5-man / group path: each top-level loot group rolls this many times.
    void OnAfterCalculateLootGroupAmount(Player const* player, Loot& loot, uint16 /*lootMode*/,
        uint32& groupAmount, LootStore const& /*store*/) override
    {
        if (!g_bossEnable)
            return;

        if (IsBossLoot(player, loot))
        {
            uint32 const before = groupAmount;
            groupAmount = BossRoll();
            if (g_bossDebug)
                LOG_INFO("server.loading", "[BossLoot dbg] GROUP hook: groupAmount {} -> {}",
                    before, groupAmount);
        }
    }

    // Raid / reference path: the referenced table is processed this many times. Override from
    // the raw item->maxcount so it never compounds with Rate.Drop.Item.ReferencedAmount.
    void OnAfterRefCount(Player const* player, LootStoreItem* item, Loot& loot, bool /*canRate*/,
        uint16 /*lootMode*/, uint32& maxcount, LootStore const& /*store*/) override
    {
        if (!g_bossEnable || !item)
            return;

        if (IsBossLoot(player, loot))
        {
            // A reference that lives inside a loot group (groupid != 0) is ALREADY
            // multiplied by the group-amount hook: under UncapChance, ProcessGuaranteed
            // loops the boosted iteration count (OnAfterCalculateLootGroupAmount) over
            // this reference. Applying BossRoll() here too compounds both boosts (Moroes:
            // a 2-drop epic pool ballooned to 8-18 instead of the intended 4-6). Only skip
            // in the UncapChance path -- with UncapChance off the group loop ignores
            // references, so this hook must remain the sole boost.
            if (item->groupid != 0 && sWorld->getBoolConfig(CONFIG_LOOT_UNCAP_CHANCE))
            {
                if (g_bossDebug)
                    LOG_INFO("server.loading",
                        "[BossLoot dbg] REF hook: ref={} grouped(groupid={}) -> skip (group hook owns it)",
                        item->reference, uint32(item->groupid));
                return;
            }

            uint32 const before = maxcount;
            maxcount = static_cast<uint32>(item->maxcount) * BossRoll();
            if (g_bossDebug)
                LOG_INFO("server.loading",
                    "[BossLoot dbg] REF hook: ref={} rawMax={} maxcount {} -> {}",
                    item->reference, uint32(item->maxcount), before, maxcount);
        }
    }
};

// --- Diagnostic: post-fill boss-loot composition (BossDebug) --------------------
// Fires AFTER the whole loot template is filled (Entries then Groups, subject to the
// MAX_NR_LOOT_ITEMS cap in Loot::AddItem). Logs the final item count, whether the cap
// was hit, and how many blues/epics survived -- to test whether multiplied junk
// (processed first) starves a boss's gear group (processed last) out of the loot list.
class boss_loot_misc : public MiscScript
{
public:
    boss_loot_misc() : MiscScript("boss_loot_misc") { }

    void OnAfterLootTemplateProcess(Loot* loot, LootTemplate const* /*tab*/, LootStore const& /*store*/,
        Player* lootOwner, bool /*personal*/, bool /*noEmptyError*/, uint16 /*lootMode*/) override
    {
        if (!g_bossDebug || !loot || !lootOwner)
            return;

        if (!IsBossLoot(lootOwner, *loot))
            return;

        uint32 blues = 0;
        uint32 epics = 0;
        uint32 greens = 0;
        for (LootItem const& li : loot->items)
        {
            if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(li.itemid))
            {
                if (proto->Quality == ITEM_QUALITY_RARE)
                    ++blues;
                else if (proto->Quality >= ITEM_QUALITY_EPIC)
                    ++epics;
                else if (proto->Quality == ITEM_QUALITY_UNCOMMON)
                    ++greens;
            }
        }

        uint32 const total = uint32(loot->items.size());
        LOG_INFO("server.loading",
            "[BossLoot dbg] POST-FILL items={}/{}{} quest={} greens={} blues={} epics={}",
            total, uint32(MAX_NR_LOOT_ITEMS),
            (total >= MAX_NR_LOOT_ITEMS ? " <== CAP HIT" : ""),
            uint32(loot->quest_items.size()), greens, blues, epics);
        for (LootItem const& li : loot->items)
            LOG_INFO("server.loading", "[BossLoot dbg]   stack itemid={} count={}",
                li.itemid, uint32(li.count));
    }
};

void AddBossLootScripts()
{
    new boss_loot_world();
    new boss_loot_global();
    new boss_loot_misc();
}
