#pragma once

// FalloutQuest Megaton NPC schedule-testing start time. This is applied only
// when creating a fresh development player session, after decoding the
// original Fallout3.esm. Do not modify the authored GameHour/TimeScale records
// or the normal save/restore and runtime clock progression.
namespace fo3devclock {
constexpr int kStartupHour=9;
constexpr int kStartupMinute=50;
constexpr float kStartupGameHour=float(kStartupHour)+float(kStartupMinute)/60.0f;
static_assert(kStartupHour>=0 && kStartupHour<24 &&
              kStartupMinute>=0 && kStartupMinute<60,
              "Invalid development startup game time");
}
