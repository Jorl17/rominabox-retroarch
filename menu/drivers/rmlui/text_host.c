#include "text_host.h"
#include "../../menu_driver.h"
#include "../../menu_input.h"
#include "../../../input/input_driver.h"
#include "../../../input/input_osk.h"
#include <string.h>

static rib_text_complete completed;
static void *completed_context;
static bool text_focus, previous_blocked;

void rib_host_text_focus(bool active)
{
   input_driver_state_t *input = input_state_get_ptr();
   if (active && !text_focus)
      previous_blocked = (input->flags & INP_FLAG_KB_MAPPING_BLOCKED) != 0;
   if (active) input->flags |= INP_FLAG_KB_MAPPING_BLOCKED;
   else if (text_focus && !previous_blocked) input->flags &= ~INP_FLAG_KB_MAPPING_BLOCKED;
   text_focus = active;
}

static void line_complete(void *unused, const char *value)
{
   rib_text_complete callback = completed;
   void *context = completed_context;
   (void)unused;
   completed = NULL;
   completed_context = NULL;
   menu_input_dialog_end();
   if (callback) callback(context, value ? value : "");
}

bool rib_host_keyboard_begin(const char *value, rib_text_complete complete, void *context)
{
   menu_input_ctx_line_t line = {0};
   if (completed) return false;
   line.cb = line_complete;
   if (!menu_input_dialog_start(&line)) return false;
   completed = complete;
   completed_context = context;
   input_state_get_ptr()->osk_ptr = 0;
   input_state_get_ptr()->osk_idx = OSK_LOWERCASE_LATIN;
   rib_host_keyboard_replace(value);
   return true;
}

void rib_host_keyboard_end(void)
{
   if (!completed) return;
   completed = NULL;
   completed_context = NULL;
   input_keyboard_line_free(input_state_get_ptr());
   menu_input_dialog_end();
   if (!text_focus && !previous_blocked)
      input_state_get_ptr()->flags &= ~INP_FLAG_KB_MAPPING_BLOCKED;
}
bool rib_host_keyboard_active(void) { return completed != NULL; }
const char *rib_host_keyboard_value(void)
{
   const char *value = input_state_get_ptr()->keyboard_line.buffer;
   return value ? value : "";
}
void rib_host_keyboard_replace(const char *value)
{
   input_driver_state_t *input = input_state_get_ptr();
   input_keyboard_line_clear(input);
   if (value && *value) input_keyboard_line_append(&input->keyboard_line, value, strlen(value));
}
const char *rib_host_keyboard_label(unsigned index)
{
   if (index >= RIB_KEYBOARD_KEYS) return "";
   const char *label = input_state_get_ptr()->osk_grid[index];
   return label ? label : "";
}
int rib_host_keyboard_focus(void) { return input_state_get_ptr()->osk_ptr; }
void rib_host_keyboard_choose(unsigned index)
{
   input_driver_state_t *input = input_state_get_ptr();
   const char *label = rib_host_keyboard_label(index);
   if (!completed || index >= RIB_KEYBOARD_KEYS || !*label) return;
   input->osk_ptr = (int)index;
   input_event_osk_append(&input->keyboard_line, &input->osk_idx,
         &input->osk_last_codepoint, &input->osk_last_codepoint_len,
         (int)index, true, label, strlen(label));
}
