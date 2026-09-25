#pragma once
#include "document_contract.hpp"

#include <string>
#include <cstring>
#include <cstdint>
#include <vector>
#include "events.h"
#include <retro_miscellaneous.h>

namespace rib {
static inline bool state_task_matches(
      bool pending, bool pending_is_save, const char *pending_path,
      int pending_slot, const char *path, int slot, bool is_save)
{
   if (!pending || pending_is_save != is_save)
      return false;
   if (path && path[0] && pending_path && pending_path[0]
         && strcmp(pending_path, path) != 0)
      return false;
   if (slot >= 0 && pending_slot >= 0 && slot != pending_slot)
      return false;
   return true;
}


using document_contract::kSlotCount;
inline bool valid_slot(int slot) { return slot >= 1 && slot <= kSlotCount; }

class Document;
class Focus;
class Status;

/* The selected slot, kept thumbnails and the current save or load task.
 * We store through the host, and match each callback to its request. */
class Slots
{
public:
   Slots(Document& document, Focus& focus, Status& status)
      : document(document), focus(focus), status(status) {}
   enum class Transfer { Save, Load };
   void reset_transfer() { transfer = {}; awaiting = {}; }
   bool transfer_pending() const { return transfer.pending; }
   bool load_available() const;
   /* Read every slot from the host, when the menu opens and when a save or a
    * load finishes. Nothing else changes them. */
   void refresh();
   /* Call once a frame. Returns the slot still waiting for its picture. */
   void follow();
   void request(Transfer kind);
   /* Handle SAVE, LOAD and the choice of a slot, one transfer at a time. There
    * is nothing to load from an empty slot. False for anything else. */
   bool handle(const Event& event);
   void notify_task(const char *path, int slot, bool is_save, bool success);
   void paint() const;
   void set_selected_slot(int slot);
   void set_slot_state(int slot, bool occupied, const char *thumbnail_path);
   void set_game_aspect(float aspect);
   int selected() const { return selected_slot; }
   bool occupied(int slot) const;
   bool has_thumbnail(int slot) const;

private:
   bool begin_transfer(Transfer kind);
   void look_at(int slot);
   /* How long we wait for the picture of a saved slot. */
   static constexpr int64_t kPictureWaitUs = 5000000;
   struct Awaiting
   {
      int slot = 0;
      int64_t until = 0;
   } awaiting;
   struct Request
   {
      bool pending = false;
      Transfer kind = Transfer::Load;
      int slot = 0;
      std::string path;
   } transfer;
   struct SlotState
   {
      bool occupied = false;
      std::string thumbnail_path;
      std::string thumbnail_version;
   };
   /* The picture of a slot, read through the libretro file layer, with UTF-8
    * paths on every platform. stat and the fopen in lodepng take ANSI code
    * page paths on Windows. Its version is its size and a CRC of its bytes,
    * which change when the player saves a new picture over the old one. Both
    * are empty when there is no file. */
   struct Picture
   {
      std::vector<unsigned char> bytes;
      std::string version;
   };
   static Picture read_picture(const std::string& path);
   static bool picture_ready(const std::string& path, const Picture& picture);
   static std::string quoted_css_path(const std::string& path);
   Document& document;
   Focus& focus;
   Status& status;
   float game_aspect = 4.0f / 3.0f;
   int selected_slot = 1;
   SlotState slots[kSlotCount];
};
}
