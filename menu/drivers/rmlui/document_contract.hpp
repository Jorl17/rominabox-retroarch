#pragma once

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
#include "document_contract.inc"
}
}
