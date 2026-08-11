#pragma once

class UsbMassStorageService {
 public:
  bool begin();
  bool isRunning() const { return running; }
  const char* lastError() const { return errorMessage; }

 private:
  bool running = false;
  const char* errorMessage = "Not started";
};

extern UsbMassStorageService USB_MASS_STORAGE;
