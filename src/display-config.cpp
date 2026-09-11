/** @fileoverview Display cutout (notch) settings from the user's INI config. */
#include "display-config.hpp"

#include <QSettings>

#include <algorithm>
#include <cmath>

namespace {
/// Physical height of the camera strip on 14"/16" MacBook Pro panels (the
/// pixels above a 16:10 content rectangle). Converted to logical via scale.
constexpr int kMacBookNotchPhysicalHeight = 74;
/// Physical width of the camera housing. Kept tight so the tab ears sit
/// against the cutout rather than leaving a dead band beside it.
constexpr int kMacBookNotchPhysicalWidth = 370;

bool isKnownNotchedMacBookMode(const QSize &physicalMode) {
  return (physicalMode.width() == 3456 && physicalMode.height() == 2234) ||
         (physicalMode.width() == 3024 && physicalMode.height() == 1964);
}

qreal defaultNotchWidth(qreal scale) {
  const qreal safeScale = std::max<qreal>(scale, 0.01);
  return kMacBookNotchPhysicalWidth / safeScale;
}

qreal defaultNotchHeight(qreal scale) {
  const qreal safeScale = std::max<qreal>(scale, 0.01);
  return kMacBookNotchPhysicalHeight / safeScale;
}

NotchMode parseNotchMode(const QString &raw) {
  const QString value = raw.trimmed().toLower();
  if (value == QStringLiteral("off") || value == QStringLiteral("never") ||
      value == QStringLiteral("0") || value == QStringLiteral("false"))
    return NotchMode::Off;
  if (value == QStringLiteral("on") || value == QStringLiteral("always") ||
      value == QStringLiteral("1") || value == QStringLiteral("true"))
    return NotchMode::On;
  return NotchMode::Auto;
}
} // namespace

DisplayConfig loadDisplayConfig(const QString &filePath) {
  DisplayConfig config;
  QSettings settings(filePath, QSettings::IniFormat);
  if (settings.contains(QStringLiteral("display/notch")))
    config.notch =
        parseNotchMode(settings.value(QStringLiteral("display/notch")).toString());
  const qreal width =
      settings.value(QStringLiteral("display/notch_width")).toReal();
  if (width > 0.0)
    config.notchWidth = width;
  const qreal height =
      settings.value(QStringLiteral("display/notch_height")).toReal();
  if (height > 0.0)
    config.notchHeight = height;
  return config;
}

TopCutout resolveTopCutout(const DisplayConfig &config,
                           const QSize &physicalMode, qreal scale,
                           qreal overlayWidth) {
  TopCutout cutout;
  if (config.notch == NotchMode::Off || overlayWidth <= 0.0)
    return cutout;

  const bool known = isKnownNotchedMacBookMode(physicalMode);
  if (config.notch == NotchMode::Auto && !known)
    return cutout;

  const qreal safeScale = scale > 0.0 ? scale : 1.0;
  cutout.width = config.notchWidth > 0.0 ? config.notchWidth
                                         : defaultNotchWidth(safeScale);
  cutout.height = config.notchHeight > 0.0 ? config.notchHeight
                                           : defaultNotchHeight(safeScale);
  // Cap so the tab ears still have room; a mis-set config should not
  // collapse both sides to nothing.
  cutout.width = std::min(cutout.width, overlayWidth * 0.6);
  if (cutout.width <= 0.0)
    cutout = {};
  return cutout;
}

QRectF topCutoutRect(const QRectF &bounds, const TopCutout &cutout) {
  if (cutout.width <= 0.0)
    return {};
  const qreal height =
      cutout.height > 0.0 ? cutout.height : bounds.height();
  const qreal left = bounds.center().x() - cutout.width / 2.0;
  return {left, bounds.top(), cutout.width, height};
}
