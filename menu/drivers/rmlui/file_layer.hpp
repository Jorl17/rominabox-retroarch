#ifndef RIB_MENU_FILE_LAYER_HPP
#define RIB_MENU_FILE_LAYER_HPP

/* How we read the documents, style sheets, fonts and pictures of the menu
 * for RmlUi: through the libretro file layer, with UTF-8 paths on every
 * platform. RmlUi reads files with fopen, and on Windows fopen cannot open a
 * file in a folder whose name has non-ASCII characters. A relative path is
 * a file next to the document that contains it, not a web-root-relative URL.
 * We use this in the menu and in the builder's preview renderer. */

#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Types.h>

#include <string>

namespace rib {

struct FileLayer : Rml::FileInterface
{
   Rml::FileHandle Open(const Rml::String& path) override;
   void Close(Rml::FileHandle file) override;
   size_t Read(void *buffer, size_t size, Rml::FileHandle file) override;
   bool Seek(Rml::FileHandle file, long offset, int origin) override;
   size_t Tell(Rml::FileHandle file) override;
   size_t Length(Rml::FileHandle file) override;
};

/* The path of a file as we pass it to RmlUi, and back. In RmlUi the path of
 * a document is a URL, and the folder of an image is the part before a '?'.
 * For a game in a folder whose name contains '?', we would look for its
 * pictures one folder above the menu. In every path we pass to RmlUi we
 * write '%' and '?' as %25 and %3F, and we reverse that for every file we
 * open. Nothing else in a path changes. */
std::string to_rml_path(const std::string& path);
std::string from_rml_path(const std::string& path);

/* What JoinPath in a SystemInterface returns: `path` relative to the
 * document at `document_path`. */
void join_menu_path(Rml::String& output, const Rml::String& document_path,
      const Rml::String& path);

}

#endif
