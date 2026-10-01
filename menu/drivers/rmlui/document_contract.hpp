#pragma once

#include <algorithm>

/* The built-in document names, shared by the player and the exporter.
 * document_contract.inc has one declaration per line on purpose, so we read
 * each one in the exporter, as in this header. A selected design may add
 * more screens and component classes. */
namespace rib {
namespace document_contract {
#define RIB_SLOT_COUNT(count) inline constexpr int kSlotCount = count;
#define RIB_CANVAS(width, height) \
   inline constexpr float kCanvasWidth = width; \
   inline constexpr float kCanvasHeight = height;
#define RIB_ELEMENT(name, value, scope, presence) inline constexpr char name[] = value;
#define RIB_CLASS(name, value, scope, presence) inline constexpr char name[] = value;
#define RIB_ATTRIBUTE(name, value, scope, presence) inline constexpr char name[] = value;
#define RIB_FACT(name, value) inline constexpr char name[] = value;
#define RIB_NOTICE(name, value, hold_ms) inline constexpr char Notice##name[] = value;
#include "document_contract.inc"

/* The density-independent pixel ratio that fits the design's canvas whole in
 * a window of this size: the player's window, and the builder preview's
 * picture. */
inline float canvas_density(int width, int height)
{
   const float density = std::min(
         static_cast<float>(width) / kCanvasWidth,
         static_cast<float>(height) / kCanvasHeight);
   return std::max(density, 0.1f);
}
}
}
