#include "file_layer.hpp"

#include <cstdio>
#include <filesystem>

#include <streams/file_stream.h>

namespace rib {

std::string to_rml_path(const std::string& path)
{
   std::string escaped;
   escaped.reserve(path.size());
   for (const char c : path)
      escaped += c == '%' ? "%25" : c == '?' ? "%3F" : std::string(1, c);
   return escaped;
}

std::string from_rml_path(const std::string& path)
{
   std::string plain;
   plain.reserve(path.size());
   for (size_t at = 0; at < path.size(); ++at)
   {
      if (path.compare(at, 3, "%25") == 0 || path.compare(at, 3, "%3F") == 0)
      {
         plain += path[at + 2] == '5' ? '%' : '?';
         at += 2;
         continue;
      }
      plain += path[at];
   }
   return plain;
}

void join_menu_path(Rml::String& output, const Rml::String& document_path,
      const Rml::String& path)
{
   // These are filesystem resources, not web-root-relative URLs.
   const auto child = std::filesystem::u8path(path);
   const auto base = std::filesystem::u8path(document_path).parent_path();
   output = (child.is_absolute() ? child : base / child).lexically_normal().u8string();
}

Rml::FileHandle FileLayer::Open(const Rml::String& path)
{
   return reinterpret_cast<Rml::FileHandle>(filestream_open(from_rml_path(path).c_str(),
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE));
}

void FileLayer::Close(Rml::FileHandle file)
{
   filestream_close(reinterpret_cast<RFILE*>(file));
}

size_t FileLayer::Read(void *buffer, size_t size, Rml::FileHandle file)
{
   const int64_t read = filestream_read(reinterpret_cast<RFILE*>(file), buffer, (int64_t)size);
   return read > 0 ? (size_t)read : 0;
}

bool FileLayer::Seek(Rml::FileHandle file, long offset, int origin)
{
   const int position = origin == SEEK_CUR ? RETRO_VFS_SEEK_POSITION_CURRENT
         : origin == SEEK_END ? RETRO_VFS_SEEK_POSITION_END
         : RETRO_VFS_SEEK_POSITION_START;
   return filestream_seek(reinterpret_cast<RFILE*>(file), offset, position) >= 0;
}

size_t FileLayer::Tell(Rml::FileHandle file)
{
   const int64_t at = filestream_tell(reinterpret_cast<RFILE*>(file));
   return at > 0 ? (size_t)at : 0;
}

size_t FileLayer::Length(Rml::FileHandle file)
{
   const int64_t size = filestream_get_size(reinterpret_cast<RFILE*>(file));
   return size > 0 ? (size_t)size : 0;
}

}
