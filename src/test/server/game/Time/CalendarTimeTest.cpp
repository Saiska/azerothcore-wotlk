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

#include "Define.h"
#include "Duration.h"
#include "GameTime.h"
#include "gtest/gtest.h"

using namespace std::chrono;

TEST(CalendarTime, NoElapsedReturnsSeed)
{
    EXPECT_EQ(GameTime::CalculateCalendarTime(Seconds(1000), Seconds(500), Seconds(500), 8.0f), Seconds(1000));
}

TEST(CalendarTime, SpeedOneIsRealTime)
{
    EXPECT_EQ(GameTime::CalculateCalendarTime(Seconds(1000), Seconds(500), Seconds(600), 1.0f), Seconds(1100));
}

TEST(CalendarTime, EightTimesAcceleration)
{
    EXPECT_EQ(GameTime::CalculateCalendarTime(Seconds(1000), Seconds(500), Seconds(600), 8.0f), Seconds(1800));
}

TEST(CalendarTime, FreezeAcrossReboot)
{
    Seconds before = GameTime::CalculateCalendarTime(Seconds(1000), Seconds(500), Seconds(600), 8.0f);
    EXPECT_EQ(before, Seconds(1800));
    Seconds afterReboot = GameTime::CalculateCalendarTime(before, Seconds(10600), Seconds(10600), 8.0f);
    EXPECT_EQ(afterReboot, Seconds(1800));
}
