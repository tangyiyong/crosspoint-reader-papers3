#include "UsbMassStorageService.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <Logging.h>
#include <soc/soc_caps.h>

#if defined(SOC_USB_OTG_SUPPORTED) && SOC_USB_OTG_SUPPORTED && defined(CONFIG_TINYUSB_MSC_ENABLED) && \
    CONFIG_TINYUSB_MSC_ENABLED && !ARDUINO_USB_MODE
#include <USB.h>
#include <USBMSC.h>
#endif

namespace {
constexpr uint16_t BLOCK_SIZE = 512;

bool mscRunning = false;
const char* mscError = "Not started";

#if defined(SOC_USB_OTG_SUPPORTED) && SOC_USB_OTG_SUPPORTED && defined(CONFIG_TINYUSB_MSC_ENABLED) && \
    CONFIG_TINYUSB_MSC_ENABLED && !ARDUINO_USB_MODE
USBMSC msc;
uint8_t scratch[BLOCK_SIZE];

bool readBytes(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  uint8_t* out = static_cast<uint8_t*>(buffer);
  uint32_t remaining = bufsize;
  uint32_t sector = lba;
  uint32_t sectorOffset = offset;

  while (remaining > 0) {
    const uint32_t maxChunk = BLOCK_SIZE - sectorOffset;
    const uint32_t chunk = (remaining < maxChunk) ? remaining : maxChunk;
    if (chunk == BLOCK_SIZE) {
      if (!Storage.readSectors(sector, out, 1)) return false;
    } else {
      if (!Storage.readSectors(sector, scratch, 1)) return false;
      memcpy(out, scratch + sectorOffset, chunk);
    }
    out += chunk;
    remaining -= chunk;
    sector++;
    sectorOffset = 0;
  }
  return true;
}

bool writeBytes(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  uint8_t* in = buffer;
  uint32_t remaining = bufsize;
  uint32_t sector = lba;
  uint32_t sectorOffset = offset;

  while (remaining > 0) {
    const uint32_t maxChunk = BLOCK_SIZE - sectorOffset;
    const uint32_t chunk = (remaining < maxChunk) ? remaining : maxChunk;
    if (chunk == BLOCK_SIZE) {
      if (!Storage.writeSectors(sector, in, 1)) return false;
    } else {
      if (!Storage.readSectors(sector, scratch, 1)) return false;
      memcpy(scratch + sectorOffset, in, chunk);
      if (!Storage.writeSectors(sector, scratch, 1)) return false;
    }
    in += chunk;
    remaining -= chunk;
    sector++;
    sectorOffset = 0;
  }
  return Storage.syncDevice();
}

int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  if (!mscRunning) return -1;
  return readBytes(lba, offset, buffer, bufsize) ? static_cast<int32_t>(bufsize) : -1;
}

int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  if (!mscRunning) return -1;
  return writeBytes(lba, offset, buffer, bufsize) ? static_cast<int32_t>(bufsize) : -1;
}

bool onStartStop(uint8_t powerCondition, bool start, bool loadEject) {
  LOG_DBG("USBMSC", "StartStop power=%u start=%d eject=%d", powerCondition, start, loadEject);
  if (!start || loadEject) {
    Storage.syncDevice();
  }
  return true;
}

bool beginUsbMsc() {
  if (mscRunning) return true;

  const uint32_t blockCount = Storage.sectorCount();
  if (blockCount == 0) {
    mscError = "SD card block device unavailable";
    LOG_ERR("USBMSC", "%s", mscError);
    return false;
  }

  msc.vendorID("CrossPt");
  msc.productID("PaperS3 SD");
  msc.productRevision("1.0");
  msc.onRead(onRead);
  msc.onWrite(onWrite);
  msc.onStartStop(onStartStop);
  msc.mediaPresent(true);
  msc.isWritable(true);

  if (!msc.begin(blockCount, BLOCK_SIZE)) {
    mscError = "USB MSC begin failed";
    LOG_ERR("USBMSC", "%s", mscError);
    return false;
  }

  USB.begin();
  mscRunning = true;
  mscError = "OK";
  LOG_INF("USBMSC", "Started: %lu sectors x %u", static_cast<unsigned long>(blockCount), BLOCK_SIZE);
  return true;
}
#else
bool beginUsbMsc() {
  mscError = "USB MSC not enabled in this build";
  LOG_ERR("USBMSC", "%s", mscError);
  return false;
}
#endif
}  // namespace

UsbMassStorageService USB_MASS_STORAGE;

bool UsbMassStorageService::begin() {
  running = beginUsbMsc();
  errorMessage = mscError;
  return running;
}
