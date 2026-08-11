#pragma once

#include <HalStorage.h>

#include <cstddef>

class FirmwareInstaller {
 public:
  enum class Error {
    OK = 0,
    NO_PARTITION,
    IMAGE_TOO_LARGE,
    INVALID_IMAGE,
    WRONG_DEVICE,
    READ_ERROR,
    OTA_BEGIN_FAILED,
    OTA_WRITE_FAILED,
    OTA_END_FAILED,
  };

  using ProgressCallback = void (*)(size_t processed, size_t total, void* ctx);

  static Error validateImageHeader(HalFile& file);
  static bool fitsUpdatePartition(size_t fileSize);

  Error installFromFile(HalFile& file, size_t fileSize, ProgressCallback cb, void* ctx);

  size_t getProcessedSize() const { return processedBytes; }
  size_t getTotalSize() const { return totalBytes; }

 private:
  // Transient internal-DRAM OTA write buffer. Stack allocation would exceed the
  // project stack budget, and PSRAM is avoided for flash-write cache safety.
  static constexpr size_t kReadBufferSize = 4096;

  size_t processedBytes = 0;
  size_t totalBytes = 0;
};
