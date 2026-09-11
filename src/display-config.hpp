/** @fileoverview Display cutout (notch) settings from the user's INI config. */
#pragma once

#include <QRectF>
#include <QSize>
#include <QString>

/** How the top cutout dodge is chosen. */
enum class NotchMode {
  /** Known MacBook modes get built-in defaults; others stay centered. */
  Auto,
  /** Always apply width/height (built-in defaults fill any unset values). */
  On,
  /** Never dodge. */
  Off,
};

/** Raw `[display]` keys. Every field is optional. */
struct DisplayConfig {
  NotchMode notch = NotchMode::Auto;
  /** Logical px; 0 means "unset" (use built-in default when dodge is active). */
  qreal notchWidth = 0.0;
  qreal notchHeight = 0.0;
};

/** Centered top exclusion in logical overlay coordinates. `width == 0`
 *  disables the dodge and keeps the existing centered chrome layout. */
struct TopCutout {
  qreal width = 0.0;
  qreal height = 0.0;
};

[[nodiscard]] DisplayConfig loadDisplayConfig(const QString &filePath);

/** Resolves the cutout for an overlay of `overlayWidth` on a monitor whose
 *  native mode is `physicalMode` at `scale`. */
[[nodiscard]] TopCutout resolveTopCutout(const DisplayConfig &config,
                                         const QSize &physicalMode,
                                         qreal scale, qreal overlayWidth);

/** Axis-aligned exclusion centered on `bounds`, height from the top edge. */
[[nodiscard]] QRectF topCutoutRect(const QRectF &bounds,
                                   const TopCutout &cutout);
