/** @fileoverview Notch cutout config resolve and left/right chrome layout. */
#include "display-config-smoke.hpp"

#include "display-config.hpp"
#include "editor.hpp"
#include "overlay-chrome.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cmath>

namespace {
bool writeFile(const QString &path, const QByteArray &contents) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) &&
         file.write(contents) == contents.size();
}

bool nearlyEqual(qreal a, qreal b, qreal eps = 0.51) {
  return std::abs(a - b) <= eps;
}

bool intersectsCutout(const QRectF &rect, const QRectF &cutout) {
  return rect.intersects(cutout);
}
} // namespace

bool runDisplayConfigSmoke(QString &error) {
  QTemporaryDir dir;
  if (!dir.isValid()) {
    error = QStringLiteral("could not create temporary directory");
    return false;
  }

  // Missing file / defaults: auto on a normal panel stays centered.
  {
    const DisplayConfig missing =
        loadDisplayConfig(dir.filePath(QStringLiteral("absent.conf")));
    if (missing.notch != NotchMode::Auto || missing.notchWidth != 0.0 ||
        missing.notchHeight != 0.0) {
      error = QStringLiteral("missing display config did not keep defaults");
      return false;
    }
    const TopCutout none =
        resolveTopCutout(missing, QSize(1920, 1080), 1.0, 1920);
    if (none.width != 0.0) {
      error = QStringLiteral("auto on non-MacBook mode should disable cutout");
      return false;
    }
  }

  // Auto on 16" MacBook mode applies built-in logical defaults.
  {
    DisplayConfig autoConfig;
    autoConfig.notch = NotchMode::Auto;
    const TopCutout cutout =
        resolveTopCutout(autoConfig, QSize(3456, 2234), 2.0, 1728);
    if (cutout.width <= 0.0 || cutout.height <= 0.0) {
      error = QStringLiteral("auto on 3456x2234 should enable a cutout");
      return false;
    }
    if (!nearlyEqual(cutout.height, 37.0) ||
        !nearlyEqual(cutout.width, 185.0)) {
      error = QStringLiteral("unexpected auto defaults %1x%2")
                  .arg(cutout.width)
                  .arg(cutout.height);
      return false;
    }
  }

  // Explicit off wins even on a MacBook mode.
  {
    const QString path = dir.filePath(QStringLiteral("off.conf"));
    if (!writeFile(path, "[display]\nnotch=off\nnotch_width=240\n")) {
      error = QStringLiteral("could not write off.conf");
      return false;
    }
    const TopCutout cutout = resolveTopCutout(loadDisplayConfig(path),
                                             QSize(3456, 2234), 2.0, 1728);
    if (cutout.width != 0.0) {
      error = QStringLiteral("notch=off should disable the cutout");
      return false;
    }
  }

  // Explicit width/height with notch=on.
  {
    const QString path = dir.filePath(QStringLiteral("on.conf"));
    if (!writeFile(path, "[display]\nnotch=on\nnotch_width=200\n"
                         "notch_height=40\n")) {
      error = QStringLiteral("could not write on.conf");
      return false;
    }
    const TopCutout cutout =
        resolveTopCutout(loadDisplayConfig(path), QSize(1920, 1080), 1.0, 1920);
    if (!nearlyEqual(cutout.width, 200.0) ||
        !nearlyEqual(cutout.height, 40.0)) {
      error = QStringLiteral("notch=on did not apply explicit size");
      return false;
    }
  }

  // Zero cutout: tab layout matches the previous centered math.
  {
    const QRect bounds(0, 0, 1728, 1117);
    const QVector<CaptureTab> centered = captureTabLayout(bounds, {});
    const QVector<CaptureTab> also = captureTabLayout(bounds);
    if (centered.size() != 4 || also.size() != 4) {
      error = QStringLiteral("expected four capture tabs");
      return false;
    }
    for (int index = 0; index < 4; ++index) {
      if (centered.at(index).rect != also.at(index).rect) {
        error = QStringLiteral("default cutout argument changed centered tabs");
        return false;
      }
    }
    const qreal span =
        centered.constLast().rect.right() - centered.constFirst().rect.left();
    const qreal mid =
        (centered.constFirst().rect.left() + centered.constLast().rect.right()) /
        2.0;
    if (!nearlyEqual(mid, bounds.center().x(), 1.0)) {
      error = QStringLiteral("centered tabs are not horizontally centered");
      return false;
    }
    if (span <= 0.0) {
      error = QStringLiteral("centered tab span is empty");
      return false;
    }
  }

  // Split tabs clear the cutout.
  {
    const QRect bounds(0, 0, 1728, 1117);
    TopCutout cutout;
    cutout.width = 240;
    cutout.height = 37;
    const QRectF exclusion = topCutoutRect(QRectF(bounds), cutout);
    const QVector<CaptureTab> tabs = captureTabLayout(bounds, cutout);
    if (tabs.size() != 4) {
      error = QStringLiteral("split layout should still have four tabs");
      return false;
    }
    if (tabs.at(0).kind != CaptureKind::Region ||
        tabs.at(1).kind != CaptureKind::Window ||
        tabs.at(2).kind != CaptureKind::Scroll ||
        tabs.at(3).kind != CaptureKind::Fullscreen) {
      error = QStringLiteral("split tab order is wrong");
      return false;
    }
    if (tabs.at(1).rect.right() >= exclusion.left() ||
        tabs.at(2).rect.left() <= exclusion.right()) {
      error = QStringLiteral("split tabs intersect the cutout");
      return false;
    }
    for (const CaptureTab &tab : tabs) {
      if (intersectsCutout(tab.rect, exclusion)) {
        error = QStringLiteral("a tab rect intersects the cutout");
        return false;
      }
    }
  }

  // Edit toolbar stays centered even when tabs dodge a cutout.
  {
    CaptureData capture;
    capture.previewSize = QSize(1728, 1117);
    capture.monitor.pixelSize = QSize(3456, 2234);
    capture.monitor.scale = 2.0;
    capture.monitor.geometry = QRect(0, 0, 1728, 1117);
    capture.source = QImage(3456, 2234, QImage::Format_ARGB32);
    capture.source.fill(Qt::black);
    CaptureEditor editor(capture);
    editor.resize(1728, 1117);

    TopCutout cutout;
    cutout.width = 240;
    cutout.height = 37;
    editor.setTopCutoutForTest(cutout);
    const QVector<QRectF> buttons = editor.toolbarButtonRectsForTest();
    if (buttons.size() < 20) {
      error = QStringLiteral("expected the full toolbar button set");
      return false;
    }
    const qreal left = buttons.constFirst().left();
    const qreal right = buttons.constLast().right();
    const qreal mid = (left + right) / 2.0;
    if (!nearlyEqual(mid, editor.width() / 2.0, 2.0)) {
      error = QStringLiteral("toolbar should stay centered with a cutout");
      return false;
    }
  }

  return true;
}
