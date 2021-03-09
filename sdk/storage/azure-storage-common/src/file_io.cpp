// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include "azure/storage/common/file_io.hpp"

#include <azure/core/platform.hpp>

#if defined(AZ_PLATFORM_POSIX)
#include <sys/stat.h>
#include <unistd.h>
#elif defined(AZ_PLATFORM_WINDOWS)
#include <io.h>
#pragma warning(push)
// warning C4996: 'fopen': This function or variable may be unsafe. Consider using fopen_s
// instead.
#pragma warning(disable : 4996)
#endif

#include <limits>
#include <stdexcept>
#include <stdio.h>

namespace Azure { namespace Storage { namespace Details {

#if defined(AZ_PLATFORM_POSIX)
  int64_t PlatformFileBodyStream::OnRead(
      uint8_t* buffer,
      int64_t count,
      Azure::Core::Context const& context)
  {
    (void)context;
    auto result = pread(
        this->m_fd,
        buffer,
        std::min(count, this->m_length - this->m_offset),
        this->m_baseOffset + this->m_offset);

    if (result < 0)
    {
      throw std::runtime_error("Reading error. (Code Number: " + std::to_string(errno) + ")");
    }

    this->m_offset += result;
    return result;
  }
#elif defined(AZ_PLATFORM_WINDOWS)
  int64_t PlatformFileBodyStream::OnRead(
      uint8_t* buffer,
      int64_t count,
      Azure::Core::Context const& context)
  {
    (void)context;
    DWORD numberOfBytesRead;
    auto o = OVERLAPPED();
    o.Offset = static_cast<DWORD>(this->m_baseOffset + this->m_offset);
    o.OffsetHigh = static_cast<DWORD>((this->m_baseOffset + this->m_offset) >> 32);

    auto result = ReadFile(
        this->m_hFile,
        buffer,
        // at most 4Gb to be read
        static_cast<DWORD>(std::min(
            static_cast<uint64_t>(0xFFFFFFFFUL),
            static_cast<uint64_t>(std::min(count, (this->m_length - this->m_offset))))),
        &numberOfBytesRead,
        &o);

    if (!result)
    {
      // Check error. of EOF, return bytes read to EOF
      auto error = GetLastError();
      if (error != ERROR_HANDLE_EOF)
      {
        throw std::runtime_error("Reading error. (Code Number: " + std::to_string(error) + ")");
      }
    }

    this->m_offset += numberOfBytesRead;
    return numberOfBytesRead;
  }
#endif

  FileReader::FileReader(const std::string& filename)
  {
    FILE* handle = fopen(filename.c_str(), "rb");
    if (handle == nullptr)
    {
      throw std::runtime_error("Failed to open file for reading.");
    }

    m_handle = handle;

#if defined(AZ_PLATFORM_WINDOWS)
    LARGE_INTEGER fileSize;
    BOOL ret = GetFileSizeEx((HANDLE)_get_osfhandle(_fileno(m_handle)), &fileSize);
    if (!ret)
    {
      fclose(m_handle);
      throw std::runtime_error("Failed to get size of file.");
    }
    m_fileSize = fileSize.QuadPart;
#elif defined(AZ_PLATFORM_POSIX)
    struct stat finfo;

    if (fstat(fileno(m_handle), &finfo))
    {
      fclose(m_handle);
      throw std::runtime_error("Failed to get size of file.");
    }
    m_fileSize = finfo.st_size;
#endif
  }

  FileReader::~FileReader() { fclose(m_handle); }

  FileWriter::FileWriter(const std::string& filename)
  {
    FILE* handle = fopen(filename.c_str(), "wb");
    if (handle == nullptr)
    {
      throw std::runtime_error("Failed to open file for writing.");
    }

    m_handle = handle;
  }

#if defined(AZ_PLATFORM_WINDOWS)
#pragma warning(pop)
#endif

  FileWriter::~FileWriter() { fclose(m_handle); }

  void FileWriter::Write(const uint8_t* buffer, int64_t length, int64_t offset)
  {
#if defined(AZ_PLATFORM_WINDOWS)

    if (length > std::numeric_limits<DWORD>::max())
    {
      throw std::runtime_error("failed to write file");
    }

    OVERLAPPED overlapped;
    std::memset(&overlapped, 0, sizeof(overlapped));
    overlapped.Offset = static_cast<DWORD>(static_cast<uint64_t>(offset));
    overlapped.OffsetHigh = static_cast<DWORD>(static_cast<uint64_t>(offset) >> 32);

    DWORD bytesWritten;
    BOOL ret = WriteFile(
        (HANDLE)_get_osfhandle(_fileno(m_handle)),
        buffer,
        static_cast<DWORD>(length),
        &bytesWritten,
        &overlapped);
    if (!ret)
    {
      throw std::runtime_error("failed to write file");
    }

#elif defined(AZ_PLATFORM_POSIX)

    if (static_cast<uint64_t>(length) > std::numeric_limits<size_t>::max()
        || offset > static_cast<int64_t>(std::numeric_limits<off_t>::max()))
    {
      throw std::runtime_error("failed to write file");
    }
    ssize_t bytesWritten
        = pwrite(fileno(m_handle), buffer, static_cast<size_t>(length), static_cast<off_t>(offset));
    if (bytesWritten != length)
    {
      throw std::runtime_error("failed to write file");
    }

#endif
  }
}}} // namespace Azure::Storage::Details
