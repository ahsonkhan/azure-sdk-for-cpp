// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#pragma once

#include <azure/core/io/body_stream.hpp>
#include <azure/core/platform.hpp>

#if defined(AZ_PLATFORM_WINDOWS)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <cstdint>
#include <string>

namespace Azure { namespace Storage {
  namespace Details {

#if defined(AZ_PLATFORM_WINDOWS)
    HANDLE GetHandle(FILE* file) { return (HANDLE)_get_osfhandle(_fileno(file)); }
#elif defined(AZ_PLATFORM_POSIX)
    int GetHandle(FILE* file) { return fileno(file); }
#endif
  }

  /**
   * @brief #Azure::IO::BodyStream providing its data from a file.
   */
  class PlatformFileBodyStream : public Azure::IO::BodyStream {
  private:
    // immutable
#if defined(AZ_PLATFORM_POSIX)
    int m_fd;
#elif defined(AZ_PLATFORM_WINDOWS)
    HANDLE m_hFile;
#endif
    int64_t m_baseOffset;
    int64_t m_length;
    // mutable
    int64_t m_offset;

    int64_t OnRead(uint8_t* buffer, int64_t count, Azure::Core::Context const& context) override;

  public:
#if defined(AZ_PLATFORM_POSIX)
    /**
     * @brief Construct from a file.
     *
     * @param fd File descriptor.
     * @param offset Offset in the file to start providing the data from.
     * @param length Length of the data, in bytes, to provide.
     */
    PlatformFileBodyStream(int fd, int64_t offset, int64_t length)
        : m_fd(fd), m_baseOffset(offset), m_length(length), m_offset(0)
    {
    }
#elif defined(AZ_PLATFORM_WINDOWS)
    /**
     * @brief Construct from a file.
     *
     * @param hFile File handle.
     * @param offset Offset in the file to start providing the data from.
     * @param length Length of the data, in bytes, to provide.
     */
    PlatformFileBodyStream(FileReader& fileReader, int64_t offset, int64_t length)
        : m_hFile(hFile), m_baseOffset(offset), m_length(length), m_offset(0)
    {
    }
#endif

    // Rewind seek back to 0
    void Rewind() override { this->m_offset = 0; }

    int64_t Length() const override { return this->m_length; };
  }

  using FileHandle
      = FILE*;

  class FileReader {
  public:
    FileReader(const std::string& filename);

    ~FileReader();

    FileHandle GetHandle() const { return m_handle; }
    int64_t GetFileSize() const { return m_fileSize; }

  private:
    FileHandle m_handle;
    int64_t m_fileSize;
  };

  class FileWriter {
  public:
    FileWriter(const std::string& filename);

    ~FileWriter();

    FileHandle GetHandle() const { return m_handle; }

    void Write(const uint8_t* buffer, int64_t length, int64_t offset);

  private:
    FileHandle m_handle;
  };

}}
} // namespace Azure::Storage::Details
