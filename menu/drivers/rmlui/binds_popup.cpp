#include "document_contract.hpp"
#include "binds_popup.hpp"
#include "elements.hpp"
#include <RmlUi/Core/ElementUtilities.h>
#include <cmath>
#include <set>

namespace rib
{
namespace
{
struct PopupLayout
{
   Rml::ElementDocument *document;
   Rml::Context *context;

   Rml::Element *rib_list_element(const char *id)
   {
      if (!document || !id || !*id)
         return nullptr;
      return document->GetElementById(id);
   }

   float rib_specified_dp(Rml::Element *element, const char *name)
   {
      return rib::specified_dp(element, name, context);
   }

   /* The width required for the widest row, in dp, at most the width in the
    * design.
    *
    * The three parts of a row are absolutely positioned, so the box has no
    * natural width. The `right` offset of the title is not the detail slot.
    * In native, the title stops 120dp from the edge and the detail is 128dp
    * wide, so a size from that offset would draw the title over the detail.
    * The width is the words of the title, a gap, and the fixed slots. While
    * the pager is open, the width is at least that of its buttons. */
   int rib_list_width(Rml::Element *list, int declared)
   {
      const char *const parts[] = {document_contract::ListRowTitle, document_contract::ListRowDetail,
            document_contract::ListRowState};
      const float density = std::max(context
            ? context->GetDensityIndependentPixelRatio() : 1.f, 0.1f);
      float widest = 0.f;
      std::vector<Rml::Element*> rows;

      rib::collect(list, document_contract::ListRow, rows);
      for (Rml::Element *row : rows)
      {
         if (rib::hidden(row))
            continue;
         float glyphs = 0.f;
         float reserved = 0.f;
         for (const char *part : parts)
         {
            std::vector<Rml::Element*> found;
            rib::collect(row, part, found);
            for (Rml::Element *element : found)
            {
               if (rib::display_none(element))
                  continue;
               const Rml::String text = element->GetInnerRML();
               if (text.empty())
                  continue;
               const float words = Rml::ElementUtilities::GetStringWidth(
                     element, text) / density;
               const float left = rib_specified_dp(element, "left");
               const float right = rib_specified_dp(element, "right");
               const float width = rib_specified_dp(element, "width");
               const Rml::Property *left_prop = element->GetProperty("left");
               const Rml::Property *right_prop = element->GetProperty("right");
               const bool left_set = left_prop
                     && left_prop->unit != Rml::Unit::KEYWORD;
               const bool right_set = right_prop
                     && right_prop->unit != Rml::Unit::KEYWORD;
               if (left_set && right_set && width <= 0.f)
                  glyphs = std::max(glyphs, left + words);
               else if (width > 0.f && right_set)
                  reserved = std::max(reserved, right + std::max(width, words));
               else
                  glyphs = std::max(glyphs, words);
            }
         }
         /* 8dp between the title's last letter and the detail's slot, so the
          * two strings do not run together. */
         const float need = glyphs + reserved + (glyphs > 0.f && reserved > 0.f ? 8.f : 0.f);
         if (need > widest)
            widest = need;
      }

      std::vector<Rml::Element*> pagers;
      rib::collect(list, document_contract::ListPager, pagers);
      if (!pagers.empty() && !rib::display_none(pagers[0]) && !rib::hidden(pagers[0]))
      {
         for (const char *cls : {document_contract::ListPagerPrev, document_contract::ListPagerCount,
               document_contract::ListPagerNext})
         {
            std::vector<Rml::Element*> found;
            rib::collect(pagers[0], cls, found);
            for (Rml::Element *button : found)
            {
               const float edge = rib_specified_dp(button, "left")
                     + rib_specified_dp(button, "width");
               if (edge > widest)
                  widest = edge;
            }
         }
      }

      if (widest <= 0.f)
         return declared;
      const int dp = (int)std::ceil(widest);
      return dp < declared ? dp : declared;
   }

   /* How far a visible child is drawn outside the border box of the list. A
    * row is 100% wide plus its border, so it extends past the box. With a
    * clamp that measured only the list, the row would end on the last pixel. */
   void rib_paint_overflow(Rml::Element *list,
         float &extra_left, float &extra_top, float &extra_right, float &extra_bottom)
   {
      extra_left = extra_top = extra_right = extra_bottom = 0.f;
      const Rml::Vector2f list_at = list->GetAbsoluteOffset(Rml::BoxArea::Border);
      const Rml::Vector2f list_size = list->GetBox().GetSize(Rml::BoxArea::Border);
      const char *classes[] = {document_contract::ListRow, document_contract::ListPager};
      for (const char *cls : classes)
      {
         std::vector<Rml::Element*> found;
         rib::collect(list, cls, found);
         for (Rml::Element *child : found)
         {
            if (rib::hidden(child))
               continue;
            const Rml::Vector2f at = child->GetAbsoluteOffset(Rml::BoxArea::Border);
            const Rml::Vector2f size = child->GetBox().GetSize(Rml::BoxArea::Border);
            extra_left = std::max(extra_left, list_at.x - at.x);
            extra_top = std::max(extra_top, list_at.y - at.y);
            extra_right = std::max(extra_right,
                  at.x + size.x - (list_at.x + list_size.x));
            extra_bottom = std::max(extra_bottom,
                  at.y + size.y - (list_at.y + list_size.y));
         }
      }
   }

   /* `left`/`top` are added to the padding edge of the offset parent (and to
    * the margin of the element), so we store a distance from that edge and not
    * from the border edge of the screen. From the border edge, the list would
    * be one border width too far right, and in a 2x window its border would
    * be on the last pixel of the window. */
   void rib_set_border_position(Rml::Element *list, float abs_x, float abs_y)
   {
      float origin_x = 0.f;
      float origin_y = 0.f;
      if (Rml::Element *parent = list->GetOffsetParent())
      {
         const Rml::Vector2f padding =
               parent->GetAbsoluteOffset(Rml::BoxArea::Padding);
         origin_x = padding.x;
         origin_y = padding.y;
      }
      const float margin_x = list->GetBox().GetEdge(
            Rml::BoxArea::Margin, Rml::BoxEdge::Left);
      const float margin_y = list->GetBox().GetEdge(
            Rml::BoxArea::Margin, Rml::BoxEdge::Top);
      list->SetProperty("left", std::to_string(
            (int)std::lround(abs_x - origin_x - margin_x)) + "px");
      list->SetProperty("top", std::to_string(
            (int)std::lround(abs_y - origin_y - margin_y)) + "px");
   }

   void rib_clamp_border(float &x, float &y, float w, float h,
         const Rml::Vector2f &screen_at, const Rml::Vector2f &screen_size)
   {
      const float margin = 8.f;
      const float min_x = screen_at.x + margin;
      const float min_y = screen_at.y + margin;
      const float max_x = screen_at.x + screen_size.x - margin;
      const float max_y = screen_at.y + screen_size.y - margin;
      if (x + w > max_x)
         x = max_x - w;
      if (x < min_x)
         x = min_x;
      if (y + h > max_y)
         y = max_y - h;
      if (y < min_y)
         y = min_y;
   }

   /* What a list must not cover.
    *
    * A callout or a stick group is a label, except the one the list is for.
    * The player is reading that box, so it counts as chrome, which the list
    * may never cover. The leader line and the hit ring are not in this table.
    * The heading, the label and button of the controller picker, the buttons
    * of the screen, the status line and the footer are chrome too. The option
    * list of the picker is display:none while a bind list is open, and we
    * skip the pager, a menu-action inside the list. */
   struct rib_keep_clear
   {
      const char *name;
      bool id;
      bool label;
   };

   static constexpr rib_keep_clear rib_keep_clear_rules[] = {
      {"control-callout", false, true},
      {document_contract::ControlGroup, false, true},
      {document_contract::MenuAction, false, false},
      {document_contract::Heading, true, false},
      {"control-picker-label", false, false},
      {"control-picker-current", false, false},
      {document_contract::ControlsStatus, true, false},
      {"footer", true, false},
   };

   bool rib_under(Rml::Element *ancestor, Rml::Element *element)
   {
      for (Rml::Element *node = element; node; node = node->GetParentNode())
         if (node == ancestor)
            return true;
      return false;
   }

   void rib_count_covered(Rml::Element *list, const char *anchor_id,
         float left, float top, float width, float height,
         int &labels, int &chrome)
   {
      labels = 0;
      chrome = 0;
      std::set<Rml::String> seen;
      for (const rib_keep_clear &rule : rib_keep_clear_rules)
      {
         std::vector<Rml::Element*> found;
         if (rule.id)
         {
            if (Rml::Element *element = document->GetElementById(rule.name))
               found.push_back(element);
         }
         else
            rib::collect(document, rule.name, found);
         for (Rml::Element *element : found)
         {
            if (!element || rib::hidden(element) || (list && rib_under(list, element)))
               continue;
            const Rml::String id = element->GetId();
            if (id.empty() || !seen.insert(id).second)
               continue;
            const Rml::Vector2f at = element->GetAbsoluteOffset(Rml::BoxArea::Border);
            const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
            if (at.x < left + width && at.x + size.x > left
                  && at.y < top + height && at.y + size.y > top)
            {
               /* The anchor matches a label rule. Count it as chrome, so we
                * prefer a spot that leaves the control being read uncovered. */
               if (rule.label && !(anchor_id && id == anchor_id))
                  ++labels;
               else
                  ++chrome;
            }
         }
      }
   }

   void place(const char *list_id, const char *anchor_id,
         int width_dp)
   {
      Rml::Element *list = rib_list_element(list_id);
      Rml::Element *anchor = document && anchor_id ? document->GetElementById(anchor_id) : nullptr;
      Rml::Element *screen = document ? document->GetElementById(document_contract::Screen) : nullptr;
      if (!list || !context)
         return;
      list->RemoveProperty("display");
      if (width_dp > 0)
         list->SetProperty("width", std::to_string(rib_list_width(list, width_dp)) + "dp");
      if (!anchor || !screen)
         return;
      context->Update();
      const Rml::Vector2f screen_at = screen->GetAbsoluteOffset(Rml::BoxArea::Border);
      const Rml::Vector2f anchor_at = anchor->GetAbsoluteOffset(Rml::BoxArea::Border);
      const Rml::Vector2f anchor_size = anchor->GetBox().GetSize(Rml::BoxArea::Border);
      const Rml::Vector2f list_size = list->GetBox().GetSize(Rml::BoxArea::Border);
      const Rml::Vector2f screen_size = screen->GetBox().GetSize(Rml::BoxArea::Border);
      float extra_left = 0.f, extra_top = 0.f, extra_right = 0.f, extra_bottom = 0.f;
      rib_paint_overflow(list, extra_left, extra_top, extra_right, extra_bottom);
      const float paint_w = list_size.x + extra_left + extra_right;
      const float paint_h = list_size.y + extra_top + extra_bottom;
      const float gap = 4.f;

      /* Beside the label, toward the pad (over the drawing), then below, then
       * above. Covering the drawing is fine. Covering another label is not. */
      float scene_cx = anchor_at.x + anchor_size.x * 0.5f;
      if (Rml::Element *scene = document->GetElementById(document_contract::ControllerScene))
      {
         const Rml::Vector2f scene_at = scene->GetAbsoluteOffset(Rml::BoxArea::Border);
         const Rml::Vector2f scene_size = scene->GetBox().GetSize(Rml::BoxArea::Border);
         scene_cx = scene_at.x + scene_size.x * 0.5f;
      }
      const float anchor_cx = anchor_at.x + anchor_size.x * 0.5f;
      const float beside_x = scene_cx >= anchor_cx
            ? anchor_at.x + anchor_size.x + gap
            : anchor_at.x - list_size.x - gap;
      const float away_x = scene_cx >= anchor_cx
            ? anchor_at.x - list_size.x - gap
            : anchor_at.x + anchor_size.x + gap;
      /* Clamping can move the spots beside and away back onto the anchor. We
       * may cover the drawing, so once each of the four sides covers something,
       * we also try the middle of the pad and the screen margins. */
      std::vector<float> columns = {
         beside_x,
         anchor_at.x,
         away_x,
         screen_at.x + 8.f,
         screen_at.x + screen_size.x - list_size.x - 8.f,
      };
      if (Rml::Element *scene = document->GetElementById(document_contract::ControllerScene))
      {
         const Rml::Vector2f scene_at = scene->GetAbsoluteOffset(Rml::BoxArea::Border);
         const Rml::Vector2f scene_size = scene->GetBox().GetSize(Rml::BoxArea::Border);
         columns.push_back(scene_at.x + gap);
         columns.push_back(scene_at.x + (scene_size.x - list_size.x) * 0.5f);
         columns.push_back(scene_at.x + scene_size.x - list_size.x - gap);
      }
      std::vector<std::pair<float, float>> spots = {
         {beside_x, anchor_at.y},
         {anchor_at.x, anchor_at.y + anchor_size.y + gap},
         {anchor_at.x, anchor_at.y - list_size.y - gap},
         {away_x, anchor_at.y},
      };

      float best_x = anchor_at.x;
      float best_y = anchor_at.y + anchor_size.y + gap;
      int best_labels = 1000000;
      int best_chrome = 1000000;
      bool best_inside = false;
      bool have = false;
      bool settled = false;
      for (size_t index = 0; index < spots.size() && !settled; ++index)
      {
         float x = spots[index].first;
         float y = spots[index].second;
         rib_clamp_border(x, y, paint_w, paint_h, screen_at, screen_size);
         /* Clamping moves the border box of the list. Children that extend past
          * it must stay inside the same margin, so we test the painted area. */
         const float paint_x = x - extra_left;
         const float paint_y = y - extra_top;
         const bool inside =
               paint_x >= screen_at.x + 8.f - 0.5f
               && paint_y >= screen_at.y + 8.f - 0.5f
               && paint_x + paint_w <= screen_at.x + screen_size.x - 8.f + 0.5f
               && paint_y + paint_h <= screen_at.y + screen_size.y - 8.f + 0.5f;
         int labels = 0;
         int chrome = 0;
         rib_count_covered(list, anchor_id, paint_x, paint_y, paint_w, paint_h,
               labels, chrome);
         if (inside && chrome == 0 && labels == 0)
         {
            best_x = x;
            best_y = y;
            settled = true;
            break;
         }
         const bool better = !have
               || (inside && !best_inside)
               || (inside == best_inside && chrome < best_chrome)
               || (inside == best_inside && chrome == best_chrome && labels < best_labels);
         if (better)
         {
            best_x = x;
            best_y = y;
            best_labels = labels;
            best_chrome = chrome;
            best_inside = inside;
            have = true;
         }
         /* We try the three sides first. Only when each of them covers a button,
          * the status line or the footer do we look for a place clear of those.
          * When we count what a side covers, we still count only labels. */
         if (index == 3 && (best_chrome > 0 || best_labels > 0))
         {
            for (const rib_keep_clear &rule : rib_keep_clear_rules)
            {
               if (rule.label)
                  continue;
               std::vector<Rml::Element*> found;
               if (rule.id)
               {
                  if (Rml::Element *element = document->GetElementById(rule.name))
                     found.push_back(element);
               }
               else
                  rib::collect(document, rule.name, found);
               for (Rml::Element *element : found)
               {
                  if (!element || rib::hidden(element) || rib_under(list, element))
                     continue;
                  const float top = element->GetAbsoluteOffset(Rml::BoxArea::Border).y;
                  for (float column : columns)
                     spots.emplace_back(column, top - paint_h - gap);
               }
            }
            const float min_y = screen_at.y + 8.f;
            const float max_y = screen_at.y + screen_size.y - 8.f - paint_h;
            for (float scan_y = min_y; scan_y <= max_y; scan_y += 24.f)
               for (float column : columns)
                  spots.emplace_back(column, scan_y);
         }
      }
      rib_set_border_position(list, best_x, best_y);
   }

   /* Relative to the screen. The labels are the callout and group elements in
    * the document. The box of a stick is control-group-l_stick, not
    * control-group- plus an axis id, so it is not in the control table. */
   int covered(const char *anchor_id,
         int left, int top, int width, int height)
   {
      if (!document || !context || width <= 0 || height <= 0)
         return 0;
      context->Update();
      Rml::Element *screen = document->GetElementById(document_contract::Screen);
      if (!screen)
         return 0;
      const Rml::Vector2f origin = screen->GetAbsoluteOffset(Rml::BoxArea::Border);
      int labels = 0;
      int chrome = 0;
      rib_count_covered(nullptr, anchor_id,
            origin.x + (float)left, origin.y + (float)top,
            (float)width, (float)height, labels, chrome);
      return labels;
   }

};
}

void place_binds_popup(Rml::ElementDocument *document, Rml::Context *context,
      const char *list_id, const char *anchor_id, int width_dp)
{
   PopupLayout{document, context}.place(list_id, anchor_id, width_dp);
}

int popup_covered_labels(Rml::ElementDocument *document, Rml::Context *context,
      const char *anchor_id, int left, int top, int width, int height)
{
   return PopupLayout{document, context}.covered(anchor_id, left, top, width, height);
}
}
