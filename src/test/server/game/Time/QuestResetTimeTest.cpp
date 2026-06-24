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

#include "Common.h"
#include "Timer.h"
#include "gtest/gtest.h"
#include <ctime>

using namespace Acore::Time;

// A fixed, TZ-independent base: decompose with localtime and assert structure.
static std::tm Decompose(time_t t)
{
    std::tm out{};
#if defined(_WIN32)
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return out;
}

TEST(QuestResetTime, DailyOnceIsNextMidnight)
{
    time_t base = std::time(nullptr);
    time_t r = GetNextDailyReset(base, 1);
    EXPECT_GT(r, base);
    EXPECT_LE(r - base, static_cast<time_t>(DAY));
    std::tm d = Decompose(r);
    EXPECT_EQ(d.tm_hour, 0);
    EXPECT_EQ(d.tm_min, 0);
    EXPECT_EQ(d.tm_sec, 0);
}

TEST(QuestResetTime, DailyTwiceIsHalfDaySlices)
{
    time_t base = std::time(nullptr);
    time_t r = GetNextDailyReset(base, 2);
    EXPECT_GT(r, base);
    EXPECT_LE(r - base, static_cast<time_t>(DAY) / 2);
    std::tm d = Decompose(r);
    EXPECT_TRUE(d.tm_hour == 0 || d.tm_hour == 12);
    EXPECT_EQ(d.tm_min, 0);
    EXPECT_EQ(d.tm_sec, 0);
}

TEST(QuestResetTime, DailyFourIsSixHourSlices)
{
    time_t r = GetNextDailyReset(std::time(nullptr), 4);
    std::tm d = Decompose(r);
    EXPECT_TRUE(d.tm_hour == 0 || d.tm_hour == 6 || d.tm_hour == 12 || d.tm_hour == 18);
    EXPECT_EQ(d.tm_min, 0);
    EXPECT_EQ(d.tm_sec, 0);
}

TEST(QuestResetTime, DailyBoundaryIsStrictlyAfter)
{
    // Land exactly on a slice, then ask again: must return the NEXT slice, never equal.
    time_t slice = GetNextDailyReset(std::time(nullptr), 4);
    time_t next  = GetNextDailyReset(slice, 4);
    EXPECT_EQ(next - slice, static_cast<time_t>(DAY) / 4);
}

TEST(QuestResetTime, DailyZeroTreatedAsOne)
{
    time_t base = std::time(nullptr);
    EXPECT_EQ(GetNextDailyReset(base, 0), GetNextDailyReset(base, 1));
}

TEST(QuestResetTime, WeeklyBaseOverloadIsThursdayMidnight)
{
    time_t base = std::time(nullptr);
    time_t r = GetNextTimeWithDayAndHour(4, 0, base);   // Thursday 00:00 after base
    EXPECT_GT(r, base);
    EXPECT_LE(r - base, static_cast<time_t>(DAY) * 7);
    std::tm d = Decompose(r);
    EXPECT_EQ(d.tm_wday, 4);
    EXPECT_EQ(d.tm_hour, 0);
}

TEST(QuestResetTime, MonthlyBaseOverloadIsFirstMidnight)
{
    time_t base = std::time(nullptr);
    time_t r = GetNextTimeWithMonthAndHour(-1, 0, base); // next month 1st 00:00 after base
    EXPECT_GT(r, base);
    std::tm d = Decompose(r);
    EXPECT_EQ(d.tm_mday, 1);
    EXPECT_EQ(d.tm_hour, 0);
}
