#include "file_layer.hpp"

#include <cstdio>
#include <filesystem>

#include <streams/file_stream.h>

namespace rib {

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
   return reinterpret_cast<Rml::FileHandle>(filestream_open(path.c_str(),
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
