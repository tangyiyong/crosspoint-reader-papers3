#include "FirmwareInstaller.h"

#include <Arduino.h>
#include <Logging.h>
#include <esp_app_format.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>

#include <algorithm>

FirmwareInstaller::Error FirmwareInstaller::validateImageHeader(HalFile& file) {
  esp_image_header_t imageHeader;
  const int bytesRead = file.read(&imageHeader, sizeof(imageHeader));
  if (bytesRead < static_cast<int>(sizeof(imageHeader))) {
    LOG_ERR("FW_INSTALL", "Failed to read image header");
    return Error::READ_ERROR;
  }

  if (imageHeader.magic != ESP_IMAGE_HEADER_MAGIC) {
    LOG_ERR("FW_INSTALL", "Invalid image magic: 0x%02x", imageHeader.magic);
    return Error::INVALID_IMAGE;
  }

  if (imageHeader.chip_id != ESP_CHIP_ID_ESP32S3) {
    LOG_ERR("FW_INSTALL", "Invalid chip_id: %u (expected ESP32-S3)", imageHeader.chip_id);
    return Error::WRONG_DEVICE;
  }

  if (!file.seekSet(0)) {
    LOG_ERR("FW_INSTALL", "Failed to rewind file after header check");
    return Error::READ_ERROR;
  }

  return Error::OK;
}

bool FirmwareInstaller::fitsUpdatePartition(const size_t fileSize) {
  const esp_partition_t* part = esp_ota_get_next_update_partition(nullptr);
  return part != nullptr && fileSize > 0 && fileSize <= part->size;
}

FirmwareInstaller::Error FirmwareInstaller::installFromFile(HalFile& file, const size_t fileSize, ProgressCallback cb,
                                                            void* ctx) {
  processedBytes = 0;
  totalBytes = fileSize;

  const esp_partition_t* part = esp_ota_get_next_update_partition(nullptr);
  if (!part) {
    LOG_ERR("FW_INSTALL", "No OTA update partition available");
    return Error::NO_PARTITION;
  }

  if (fileSize == 0) {
    LOG_ERR("FW_INSTALL", "Empty firmware image");
    return Error::INVALID_IMAGE;
  }

  if (fileSize > part->size) {
    LOG_ERR("FW_INSTALL", "Image too large for partition: %u > %u", static_cast<unsigned>(fileSize),
            static_cast<unsigned>(part->size));
    return Error::IMAGE_TOO_LARGE;
  }

  const Error headerErr = validateImageHeader(file);
  if (headerErr != Error::OK) {
    return headerErr;
  }

  esp_ota_handle_t handle = 0;
  esp_err_t err = esp_ota_begin(part, fileSize, &handle);
  if (err != ESP_OK) {
    LOG_ERR("FW_INSTALL", "esp_ota_begin failed: %s", esp_err_to_name(err));
    return Error::OTA_BEGIN_FAILED;
  }

  uint8_t* buf = static_cast<uint8_t*>(heap_caps_malloc(kReadBufferSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (!buf) {
    LOG_ERR("FW_INSTALL", "heap_caps_malloc failed: %u bytes", static_cast<unsigned>(kReadBufferSize));
    esp_ota_abort(handle);
    return Error::OTA_WRITE_FAILED;
  }

  size_t written = 0;
  while (written < fileSize) {
    const size_t toRead = std::min(kReadBufferSize, fileSize - written);
    const int n = file.read(buf, toRead);
    if (n <= 0) {
      LOG_ERR("FW_INSTALL", "SD read failed at offset %u", static_cast<unsigned>(written));
      esp_ota_abort(handle);
      heap_caps_free(buf);
      return Error::READ_ERROR;
    }

    err = esp_ota_write(handle, buf, static_cast<size_t>(n));
    if (err != ESP_OK) {
      LOG_ERR("FW_INSTALL", "esp_ota_write failed: %s", esp_err_to_name(err));
      esp_ota_abort(handle);
      heap_caps_free(buf);
      return Error::OTA_WRITE_FAILED;
    }

    written += static_cast<size_t>(n);
    processedBytes = written;

    if (cb) {
      cb(written, fileSize, ctx);
    }

    yield();
  }

  heap_caps_free(buf);
  buf = nullptr;

  err = esp_ota_end(handle);
  if (err != ESP_OK) {
    LOG_ERR("FW_INSTALL", "esp_ota_end failed: %s", esp_err_to_name(err));
    return Error::OTA_END_FAILED;
  }

  err = esp_ota_set_boot_partition(part);
  if (err != ESP_OK) {
    LOG_ERR("FW_INSTALL", "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
    return Error::OTA_END_FAILED;
  }

  LOG_INF("FW_INSTALL", "Firmware install complete (%u bytes)", static_cast<unsigned>(fileSize));
  return Error::OK;
}
