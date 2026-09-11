/** @fileoverview Shared overlay chrome (see overlay-chrome.hpp). */
#include "overlay-chrome.hpp"

#include <QFont>
#include <QFontMetricsF>
#include <QString>
#include <QStringList>
#include <QPainter>

#include <algorithm>

QFont chromeFont(int pixelSize, bool bold) {
  static const QFont base = [] {
    QFont font;
    font.setFamilies({QStringLiteral("Adwaita Sans"),
                      QStringLiteral("Noto Sans")});
    return font;
  }();
  QFont font = base;
  font.setPixelSize(pixelSize);
  font.setBold(bold);
  return font;
}

QFont chromeDefaultFont() {
  QFont font;
  font.setFamilies({QStringLiteral("Adwaita Sans"),
                    QStringLiteral("Noto Sans")});
  font.setPointSize(11);
  return font;
}

QFont chromeMonoFont(int pixelSize, bool bold) {
  static const QFont base = [] {
    QFont font;
    font.setFamilies({QStringLiteral("monospace")});
    return font;
  }();
  QFont font = base;
  font.setPixelSize(pixelSize);
  font.setBold(bold);
  return font;
}

QString captureTabLabel(CaptureKind kind) {
  switch (kind) {
  case CaptureKind::Region:
    return QStringLiteral("REGION");
  case CaptureKind::Scroll:
    return QStringLiteral("SCROLLING REGION");
  case CaptureKind::Window:
    return QStringLiteral("WINDOW");
  case CaptureKind::Fullscreen:
    return QStringLiteral("FULLSCREEN");
  }
  return {};
}

namespace {
QFont captureTabFont() {
  QFont font(QStringLiteral("Noto Sans"));
  font.setBold(true);
  font.setPixelSize(11);
  return font;
}
QColor captureTabAccent(CaptureKind kind) {
  switch (kind) {
  case CaptureKind::Window:
    return QColor(QStringLiteral("#ffd60a"));
  case CaptureKind::Fullscreen:
    return QColor(QStringLiteral("#0a84ff"));
  case CaptureKind::Region:
  case CaptureKind::Scroll:
    break;
  }
  return QColor(QStringLiteral("#30d158"));
}
} // namespace

QVector<CaptureTab> captureTabLayout(const QRect &bounds,
                                     const TopCutout &cutout) {
  static const CaptureKind order[] = {CaptureKind::Region, CaptureKind::Window,
                                      CaptureKind::Scroll,
                                      CaptureKind::Fullscreen};
  const QFontMetricsF metrics(captureTabFont());
  constexpr qreal kPad = 14.0;
  constexpr qreal kGap = 2.0;
  constexpr qreal kHeight = 26.0;
  // Label sits kTabBarLabelPad below the top of the metric bar so it reads
  // centered; the matching bottom pad in drawCaptureTabs keeps the painted
  // wrapper on kCaptureTabBarBottom.
  constexpr qreal kTop = kCaptureTabBarBottom - kHeight - kTabBarLabelPad;
  QVector<CaptureTab> tabs;
  qreal widths[4];
  qreal total = 0.0;
  for (int index = 0; index < 4; ++index) {
    widths[index] =
        metrics.horizontalAdvance(captureTabLabel(order[index])) + 2 * kPad;
    total += widths[index] + kGap;
  }
  total -= kGap;

  auto placeCluster = [&](int begin, int end, qreal left) {
    qreal x = left;
    for (int index = begin; index < end; ++index) {
      tabs.push_back(
          {order[index], QRectF(x, kTop, widths[index], kHeight)});
      x += widths[index] + kGap;
    }
  };

  if (cutout.width <= 0.0) {
    placeCluster(0, 4, bounds.left() + (bounds.width() - total) / 2.0);
    return tabs;
  }

  const QRectF exclusion = topCutoutRect(QRectF(bounds), cutout);
  const qreal leftWidth = widths[0] + kGap + widths[1];
  const qreal rightWidth = widths[2] + kGap + widths[3];
  qreal leftStart = exclusion.left() - leftWidth;
  qreal rightStart = exclusion.right();
  constexpr qreal kSideMargin = 8.0;
  leftStart = std::max(bounds.left() + kSideMargin, leftStart);
  if (rightStart + rightWidth > bounds.right() - kSideMargin)
    rightStart = bounds.right() - kSideMargin - rightWidth;
  // If the ears collide (tiny overlay / huge cutout), fall back to the
  // centered strip so tabs stay usable.
  if (leftStart + leftWidth > rightStart) {
    placeCluster(0, 4, bounds.left() + (bounds.width() - total) / 2.0);
    return tabs;
  }
  placeCluster(0, 2, leftStart);
  placeCluster(2, 4, rightStart);
  return tabs;
}

int captureTabAt(const QVector<CaptureTab> &tabs, const QPointF &position) {
  for (int index = 0; index < tabs.size(); ++index) {
    if (tabs.at(index).rect.adjusted(-2, -6, 2, 6).contains(position))
      return index;
  }
  return -1;
}

void drawCaptureTabs(QPainter &painter, const QVector<CaptureTab> &tabs,
                     CaptureKind active, const QPointF &cursor) {
  if (tabs.isEmpty())
    return;
  // Hangs off the top edge like a tab strip: square at the top (drawn past
  // the edge so only the bottom corners round), not a floating pill. Split
  // ears keep the notch-facing edge square so they meet the housing cleanly.
  constexpr qreal kJoinSlop = 6.0;
  constexpr qreal kRadius = 12.0;
  painter.setFont(captureTabFont());
  const bool split = tabs.size() >= 2 &&
                     tabs.constLast().rect.left() -
                             tabs.constFirst().rect.right() >
                         kJoinSlop;
  int clusterStart = 0;
  int clusterIndex = 0;
  while (clusterStart < tabs.size()) {
    int clusterEnd = clusterStart;
    while (clusterEnd + 1 < tabs.size() &&
           tabs.at(clusterEnd + 1).rect.left() -
                   tabs.at(clusterEnd).rect.right() <=
               kJoinSlop)
      ++clusterEnd;
    QRectF bar = tabs.at(clusterStart)
                     .rect.united(tabs.at(clusterEnd).rect)
                     .adjusted(-5, -30, 5, kTabBarLabelPad);
    // Reach a couple of pixels into the cutout so the ear and housing share
    // an edge instead of leaving a hairline gap from antialiasing.
    if (split) {
      if (clusterIndex == 0)
        bar.setRight(bar.right() + 3.0);
      else
        bar.setLeft(bar.left() - 3.0);
    }
    // Centered strip keeps a faint edge so it reads as chrome over the dim.
    // Notch ears drop the stroke so they read as continuous with the menu bar.
    if (split)
      painter.setPen(Qt::NoPen);
    else
      painter.setPen(QPen(QColor(255, 255, 255, 32), 1));
    painter.setBrush(QColor(18, 18, 22, 235));
    if (!split) {
      painter.drawRoundedRect(bar, kRadius, kRadius);
    } else {
      const bool outerLeft = clusterIndex == 0;
      QPainterPath path;
      const qreal l = bar.left();
      const qreal t = bar.top();
      const qreal r = bar.right();
      const qreal b = bar.bottom();
      path.moveTo(l, t);
      path.lineTo(r, t);
      if (outerLeft) {
        path.lineTo(r, b);
        path.lineTo(l + kRadius, b);
        path.quadTo(l, b, l, b - kRadius);
        path.lineTo(l, t);
      } else {
        path.lineTo(r, b - kRadius);
        path.quadTo(r, b, r - kRadius, b);
        path.lineTo(l, b);
        path.lineTo(l, t);
      }
      path.closeSubpath();
      painter.drawPath(path);
    }
    clusterStart = clusterEnd + 1;
    ++clusterIndex;
  }
  const int hovered = captureTabAt(tabs, cursor);
  for (int index = 0; index < tabs.size(); ++index) {
    const CaptureTab &tab = tabs.at(index);
    painter.setPen(Qt::NoPen);
    if (tab.kind == active) {
      painter.setBrush(captureTabAccent(tab.kind));
      painter.drawRoundedRect(tab.rect, 9, 9);
      painter.setPen(QColor(18, 18, 22));
    } else {
      if (index == hovered) {
        painter.setBrush(QColor(255, 255, 255, 28));
        painter.drawRoundedRect(tab.rect, 9, 9);
      }
      painter.setPen(QColor(255, 255, 255, index == hovered ? 255 : 190));
    }
    painter.drawText(tab.rect, Qt::AlignCenter, captureTabLabel(tab.kind));
  }
}

QRectF drawModeBadge(QPainter &painter, const QRect &bounds,
                     const QString &label, const QColor &accent,
                     QRectF *closeRect) {
  QFont badgeFont(QStringLiteral("Noto Sans"));
  badgeFont.setBold(true);
  badgeFont.setPixelSize(11);
  painter.setFont(badgeFont);
  const QString badge = label + QStringLiteral("  ×");
  const int badgeWidth = painter.fontMetrics().horizontalAdvance(badge) + 24;
  const QRectF badgeRect((bounds.width() - badgeWidth) / 2.0, 12, badgeWidth,
                         32);
  painter.setPen(QPen(QColor(255, 255, 255, 32), 1));
  painter.setBrush(QColor(18, 18, 22, 235));
  painter.drawRoundedRect(badgeRect, 10, 10);
  painter.setPen(accent);
  painter.drawText(badgeRect, Qt::AlignCenter, badge);
  if (closeRect) {
    // The × and a little around it, so the click that closes has a target
    // rather than a pixel.
    const qreal closeWidth =
        painter.fontMetrics().horizontalAdvance(QStringLiteral("×")) + 18;
    *closeRect = QRectF(badgeRect.right() - closeWidth, badgeRect.top(),
                        closeWidth, badgeRect.height());
  }
  return badgeRect;
}

void drawHotkeyLegend(QPainter &painter, const QRect &bounds,
                      const QVector<QPair<QString, QString>> &entries) {
  if (entries.isEmpty())
    return;
  const QFont font = chromeFont(11);
  const QFontMetricsF metrics(font);
  constexpr qreal keyGap = 10;    // between a key and what it does
  constexpr qreal marginLeft = 14;
  constexpr qreal marginBottom = 14;
  constexpr qreal rowHeight = 17;
  qreal keyWidth = 0.0;
  for (const auto &entry : entries)
    keyWidth = std::max(keyWidth, metrics.horizontalAdvance(entry.first));
  painter.setFont(font);
  // Bottom-left, growing upward: entry 0 is the bottom-most row. No card, no
  // border, no dodging the pointer or anything else — the caller draws this
  // early, so real chrome painted afterward simply covers it where the two
  // overlap, and the low opacity keeps it out of the way where nothing does.
  for (int index = 0; index < entries.size(); ++index) {
    const qreal y =
        bounds.height() - marginBottom - (index + 1) * rowHeight;
    painter.setPen(QColor(169, 182, 203, 165));
    painter.drawText(QRectF(marginLeft, y, keyWidth, rowHeight - 2),
                     Qt::AlignLeft | Qt::AlignVCenter, entries.at(index).first);
    painter.setPen(QColor(199, 204, 214, 130));
    painter.drawText(
        QRectF(marginLeft + keyWidth + keyGap, y,
               bounds.width() - marginLeft - keyWidth - keyGap - 14,
               rowHeight - 2),
        Qt::AlignLeft | Qt::AlignVCenter, entries.at(index).second);
  }
}

void drawStatusPill(QPainter &painter, const QRect &bounds,
                    const QString &text) {
  QFont font(QStringLiteral("Noto Sans"));
  font.setPixelSize(13);
  painter.setFont(font);
  const int width = painter.fontMetrics().horizontalAdvance(text) + 28;
  const QRectF pill((bounds.width() - width) / 2.0, bounds.height() - 42.0,
                    width, 30);
  painter.setPen(QPen(QColor(255, 255, 255, 32), 1));
  painter.setBrush(QColor(18, 18, 22, 232));
  painter.drawRoundedRect(pill, 10, 10);
  painter.setPen(Qt::white);
  painter.drawText(pill, Qt::AlignCenter, text);
}
