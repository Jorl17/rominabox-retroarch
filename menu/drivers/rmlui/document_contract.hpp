#pragma once

/* The built-in document names, shared by the player and the composition tests.
 * document_contract.inc has one declaration per line on purpose, so we read
 * each one in the Rust contract test without scanning implementation source.
 * A selected design may add more screens and component classes. */
namespace rib {
namespace document_contract {
#define RIB_SLOT_COUNT(count) inline constexpr int kSlotCount = count;
#define RIB_ELEMENT(name, value, scope, presence) inline constexpr char name[] = value;
#define RIB_CLASS(name, value, scope, presence) inline constexpr char name[] = value;
#define RIB_ATTRIBUTE(name, value, scope, presence) inline constexpr char name[] = value;
#define RIB_FACT(name, value) inline constexpr char name[] = value;
#include "document_contract.inc"
#undef RIB_FACT
#undef RIB_ATTRIBUTE
#undef RIB_CLASS
#undef RIB_ELEMENT
#undef RIB_SLOT_COUNT
}
}
