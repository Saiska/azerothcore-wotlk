/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Chat.h"
#include "CommandScript.h"
#include "GameTime.h"
#include "RBAC.h"
#include "ScriptMgr.h"
#include "Timer.h"
#include "World.h"

using namespace Acore::ChatCommands;

class realmtime_commandscript : public CommandScript
{
public:
    realmtime_commandscript() : CommandScript("realmtime_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "realmtime", HandleRealmTimeCommand, rbac::RBAC_PERM_COMMAND_REALMTIME, Console::Yes },
        };
        return commandTable;
    }

    static bool HandleRealmTimeCommand(ChatHandler* handler)
    {
        std::tm lt = Acore::Time::TimeBreakdown(GameTime::GetCalendarTime().count());
        handler->PSendSysMessage("Realm calendar: {:04}-{:02}-{:02} {:02}:{:02}  (speed x{})",
            lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min,
            sWorld->getFloatConfig(CONFIG_FLOAT_REALM_TIME_SPEED));
        return true;
    }
};

void AddSC_realmtime_commandscript()
{
    new realmtime_commandscript();
}
