/* Windows text services for RmlUi: the clipboard, and the position of the
 * composition window of the input method. The characters arrive as RetroArch
 * keyboard events with the text composed in Windows (WM_CHAR), and we pass
 * them to RmlUi from the text entry, with no key-to-character table. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <imm.h>
#include <algorithm>
#include <cstring>
#include <string>
#include "text_input_platform.hpp"
#include "document.hpp"
#include "host.h"

namespace {
std::wstring wide(const Rml::String& text)
{
   const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
   std::wstring result(size > 0 ? size - 1 : 0, L'\0');
   if (size > 1)
      MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &result[0], size);
   return result;
}

Rml::String utf8(const wchar_t *text)
{
   const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
   Rml::String result(size > 0 ? size - 1 : 0, '\0');
   if (size > 1)
      WideCharToMultiByte(CP_UTF8, 0, text, -1, &result[0], size, nullptr, nullptr);
   return result;
}

/* Line ends are CRLF in the clipboard and LF in the editor, as on every
 * platform. */
void replace_all(Rml::String& text, const char *from, const char *to)
{
   const size_t length = strlen(from);
   for (size_t at = text.find(from); at != Rml::String::npos; at = text.find(from, at + strlen(to)))
      text.replace(at, length, to);
}

class WindowsTextInput final : public rib::TextInputPlatform
{
public:
   explicit WindowsTextInput(rib::Document& document) : document(document) {}

   void get_clipboard(Rml::String& text) const override
   {
      text.clear();
      if (!OpenClipboard(window()))
         return;
      if (HANDLE data = GetClipboardData(CF_UNICODETEXT))
         if (const wchar_t *value = static_cast<const wchar_t*>(GlobalLock(data)))
         {
            text = utf8(value);
            GlobalUnlock(data);
         }
      CloseClipboard();
      replace_all(text, "\r\n", "\n");
   }

   void set_clipboard(const Rml::String& text) override
   {
      Rml::String lines = text;
      replace_all(lines, "\n", "\r\n");
      const std::wstring value = wide(lines);
      const size_t bytes = (value.size() + 1) * sizeof(wchar_t);
      HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
      if (!memory)
         return;
      if (void *target = GlobalLock(memory))
      {
         memcpy(target, value.c_str(), bytes);
         GlobalUnlock(memory);
      }
      if (!OpenClipboard(window()))
      {
         GlobalFree(memory);
         return;
      }
      EmptyClipboard();
      /* After this call the memory belongs to the clipboard. */
      if (!SetClipboardData(CF_UNICODETEXT, memory))
         GlobalFree(memory);
      CloseClipboard();
   }

   void caret(Rml::Vector2f position, float line_height) override
   {
      HWND hwnd = window();
      Rml::Context *context = document.get_context();
      RECT client;
      if (!hwnd || !context || !GetClientRect(hwnd, &client))
         return;
      /* We lay out the menu at the size of the context and draw it over the
       * whole client area. */
      const Rml::Vector2i size = context->GetDimensions();
      COMPOSITIONFORM form = {};
      form.dwStyle = CFS_POINT;
      form.ptCurrentPos.x = (LONG)(position.x * client.right / std::max(1, size.x));
      form.ptCurrentPos.y = (LONG)((position.y + line_height) * client.bottom / std::max(1, size.y));
      if (HIMC ime = ImmGetContext(hwnd))
      {
         ImmSetCompositionWindow(ime, &form);
         ImmReleaseContext(hwnd, ime);
      }
   }

private:
   static HWND window() { return (HWND)rib_host_native_window(); }
   rib::Document& document;
};
}

namespace rib {
std::unique_ptr<TextInputPlatform> make_text_input_platform(Document& document)
{
   return std::make_unique<WindowsTextInput>(document);
}
}
