#pragma once

namespace rib {
inline constexpr int kSlotCount = 6;
inline bool valid_slot(int slot) { return slot >= 1 && slot <= kSlotCount; }
}
