#pragma once

#include <GfxRenderer.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/Activity.h"

namespace OrientationHelper {

inline GfxRenderer::Orientation toRendererOrientation(uint8_t orientation) {
  switch (orientation) {
    case CrossPointSettings::LANDSCAPE_CW:
      return GfxRenderer::Orientation::LandscapeClockwise;
    case CrossPointSettings::INVERTED:
      return GfxRenderer::Orientation::PortraitInverted;
    case CrossPointSettings::LANDSCAPE_CCW:
      return GfxRenderer::Orientation::LandscapeCounterClockwise;
    case CrossPointSettings::PORTRAIT:
    default:
      return GfxRenderer::Orientation::Portrait;
  }
}

inline uint8_t uiOrientationToReaderOrientation(uint8_t uiOrientation) {
  return uiOrientation == CrossPointSettings::UI_INVERTED ? CrossPointSettings::INVERTED : CrossPointSettings::PORTRAIT;
}

inline uint8_t effectiveOrientationFor(const Activity* activity) {
  if (activity && (activity->supportsLandscape() || activity->isReaderActivity())) {
#if CROSSPOINT_PAPERS3
    return CrossPointSettings::normalizePaperS3Orientation(SETTINGS.orientation);
#else
    return SETTINGS.orientation;
#endif
  }
  return uiOrientationToReaderOrientation(SETTINGS.uiOrientation);
}

inline void applyOrientation(GfxRenderer& renderer, MappedInputManager& input, const Activity* activity) {
  const uint8_t orientation = effectiveOrientationFor(activity);
  renderer.setOrientation(toRendererOrientation(orientation));
  input.setTouchOrientation(orientation);
}

}  // namespace OrientationHelper
