#include "Config.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"

namespace
{
    bool  g_enable      = true;
    float g_healthMult  = 1.20f;
    float g_manaMult    = 1.20f;
}

// Loads config on boot and on `.reload config`.
class mod_player_vitals_world : public WorldScript
{
public:
    mod_player_vitals_world() : WorldScript("mod_player_vitals_world") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        g_enable     = sConfigMgr->GetOption<bool>("VitalsBoost.Enable", true);
        g_healthMult = sConfigMgr->GetOption<float>("VitalsBoost.Health.Multiplier", 1.20f);
        g_manaMult   = sConfigMgr->GetOption<float>("VitalsBoost.Mana.Multiplier", 1.20f);
        LOG_INFO("server.loading", "VitalsBoost: enable={} health={} mana={}",
                 g_enable, g_healthMult, g_manaMult);
    }
};

// Multiplies the recalculated pools in place (players AND bots — both are Player).
class mod_player_vitals_player : public PlayerScript
{
public:
    mod_player_vitals_player() : PlayerScript("mod_player_vitals_player") { }

    void OnPlayerAfterUpdateMaxHealth(Player* /*player*/, float& value) override
    {
        if (g_enable)
            value *= g_healthMult;
    }

    void OnPlayerAfterUpdateMaxPower(Player* /*player*/, Powers& power, float& value) override
    {
        if (g_enable && power == POWER_MANA)
            value *= g_manaMult;
    }
};

void AddPlayerVitalsScripts()
{
    new mod_player_vitals_world();
    new mod_player_vitals_player();
}
