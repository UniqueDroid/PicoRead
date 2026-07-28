#include "SleepFrameManager.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalStorage.h>

void SleepFrameManager::save() {
  HalFile file;
  if (!Storage.openFileForWrite("SLP", PATH, file)) return;
  file.write(_renderer.getFrameBuffer(), _renderer.getBufferSize());
}

bool SleepFrameManager::restore() {
  HalFile file;
  if (!Storage.openFileForRead("SLP", PATH, file)) return false;

  const size_t bufferSize = _display.getBufferSize();
  const size_t bytesRead = file.read(_display.getFrameBuffer(), bufferSize);
  file.close();  // must close before Storage.remove() on the same path

  if (bytesRead != bufferSize) {
    Storage.remove(PATH);
    return false;
  }
  Storage.remove(PATH);
  return true;
}
