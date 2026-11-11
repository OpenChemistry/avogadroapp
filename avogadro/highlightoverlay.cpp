/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "highlightoverlay.h"

#include <QtCore/QEvent>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPolygonF>
#include <QtWidgets/QStyle>

#include <algorithm>
#include <cmath>

namespace Avogadro {

namespace {
// Repaint interval while pulsing (about 25 frames a second), in milliseconds.
constexpr int kPulseTickMs = 40;
// With reduce motion nothing animates, so the timer only needs to notice that
// the target moved, or (after kStaticRevealMs) that the thick ring should end.
constexpr int kStaticTickMs = 250;
// One full cycle of the ring's brightness pulse, in milliseconds.
constexpr double kPulsePeriodMs = 1200.0;
// A reveal starts with the ring enlarged and shrinks it over this long (ms).
constexpr double kRevealGrowMs = 400.0;
constexpr double kRevealGrowPixels = 10.0;
// With reduce motion, how long a reveal keeps the thicker static ring (ms).
constexpr qint64 kStaticRevealMs = 2000;
// Pen widths in pixels: the normal ring, and the reduce-motion reveal ring.
constexpr int kRingWidth = 3;
constexpr int kThickRingWidth = 5;
constexpr int kHaloExtra = 2; // each side
// Gap, in pixels, between the target and the ring when not enlarged.
constexpr int kRingMargin = 3;
constexpr int kMaxGrow = 14; // farthest the ring reaches past the target
constexpr double kTwoPi = 6.28318530717958647692;
// Longest callout kept, counting a terminating null as in a C buffer (so 511
// characters). The text is laid out on every tick, so cap the work.
constexpr int kMaxCalloutLength = 512;

// std::clamp is undefined when lo > hi, which happens when the bubble is too
// narrow (under 24 pixels) for the pointer's inset; centre in that case.
int clampInt(int value, int lo, int hi)
{
  if (lo > hi)
    return (lo + hi) / 2;
  return std::max(lo, std::min(hi, value));
}
} // namespace

HighlightOverlay::HighlightOverlay(QWidget* window) : QWidget(window)
{
  // Clicks go straight through to the real interface, and the overlay never
  // takes focus.
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_ShowWithoutActivating);
  setFocusPolicy(Qt::NoFocus);

  // Not started until there is a target to follow.
  connect(&m_timer, &QTimer::timeout, this, &HighlightOverlay::tick);

  // The overlay is a child covering the window; watch the window to follow
  // its resizes.
  if (window != nullptr)
    window->installEventFilter(this);
  syncToWindow();
  hide();
}

void HighlightOverlay::showTarget(const Locator& locator,
                                  const QString& callout, bool reveal,
                                  bool reduceMotion)
{
  // Remember what was painted before, so that region can be repainted too if
  // the new target is somewhere else.
  const QRect oldArea = paintedArea();
  m_locator = locator;
  m_callout = callout;
  // The callout text may come from an RPC client; bound the layout work.
  if (m_callout.size() > kMaxCalloutLength - 1) {
    int keep = kMaxCalloutLength - 1;
    // Don't split a surrogate pair.
    if (m_callout.at(keep - 1).isHighSurrogate())
      --keep;
    m_callout.truncate(keep);
  }
  m_reveal = reveal;
  m_reduceMotion = reduceMotion;
  m_clock.start(); // restarts the pulse and the reveal enlargement

  syncToWindow();
  // Forget the old position so relocate() always repaints.
  m_target = QRect();
  relocate();
  show();
  // Siblings created after the overlay would otherwise draw over it.
  raise();
  update(oldArea);
  update(paintedArea());

  // The timer both animates and keeps the outline on a target that moves
  // (the window is dragged, a dock floats, the view is resized).
  m_timer.setInterval(reduceMotion ? kStaticTickMs : kPulseTickMs);
  m_timer.start();
}

void HighlightOverlay::clearTarget()
{
  // Invalidate before the state is cleared, while paintedArea() still knows
  // what was drawn.
  update(paintedArea());
  m_locator = nullptr;
  m_target = QRect();
  m_callout.clear();
  m_timer.stop();
  hide();
}

bool HighlightOverlay::eventFilter(QObject* watched, QEvent* event)
{
  if (watched == parentWidget()) {
    switch (event->type()) {
      case QEvent::Resize:
      case QEvent::Show:
        // Keep covering the window, and move the ring in case the layout
        // changed under the target.
        syncToWindow();
        relocate();
        break;
      case QEvent::ChildAdded:
        // New children (e.g. a re-created panel) stack on top; get back above
        // them once they are in place. Deferred because the child is not fully
        // constructed when ChildAdded arrives.
        if (isVisible())
          QTimer::singleShot(0, this, [this]() { raise(); });
        break;
      default:
        break;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void HighlightOverlay::changeEvent(QEvent* event)
{
  // The colours are read from the palette every time we paint; the callout
  // size follows the font, so a font change needs a repaint too.
  if (event->type() == QEvent::PaletteChange ||
      event->type() == QEvent::FontChange)
    update();
  QWidget::changeEvent(event);
}

// The overlay is always exactly the window's size, so its coordinates are the
// window's client coordinates.
void HighlightOverlay::syncToWindow()
{
  if (QWidget* host = parentWidget())
    setGeometry(host->rect());
}

// The locator returns global (screen) coordinates; mapFromGlobal() turns the
// corner into this widget's coordinates (the size is unchanged). An empty
// result means the target is gone or hidden: nothing is outlined, but the
// callout stays.
void HighlightOverlay::relocate()
{
  if (!m_locator)
    return;
  const QRect global = m_locator();
  const QRect local = global.isEmpty()
                        ? QRect()
                        : QRect(mapFromGlobal(global.topLeft()), global.size());
  if (local == m_target)
    return;
  // Repaint both where it was and where it is now.
  update(paintedArea());
  m_target = local;
  update(paintedArea());
}

void HighlightOverlay::tick()
{
  relocate();
  update(paintedArea());
}

// The largest the ring can be, not the current size, so one rectangle is
// enough for invalidation and for placing the callout clear of the ring.
QRect HighlightOverlay::ringRect(const QRect& target) const
{
  if (target.isEmpty())
    return QRect();
  return target.adjusted(-kMaxGrow, -kMaxGrow, kMaxGrow, kMaxGrow);
}

// Chooses the bubble's rectangle in local coordinates: sized to the wrapped
// text, placed beside the target if it fits, and clamped inside the window.
QRect HighlightOverlay::calloutRect(const QRect& target) const
{
  if (m_callout.isEmpty())
    return QRect();

  // Sizes follow the font, so the callout scales with the user's text size.
  const QFontMetrics metrics(font());
  const int padding = std::max(6, metrics.height() / 2);
  const int arrow = std::max(8, metrics.height() * 2 / 3);
  // Keep a few pixels clear of the window edge.
  const QRect area = rect().adjusted(4, 4, -4, -4);
  // About 18 line-heights wide, so the text wraps rather than running across
  // the window.
  const int maxWidth = std::min(metrics.height() * 18, area.width());

  // Measure the wrapped text; the height of 2000 is just "tall enough".
  const QRect text =
    metrics.boundingRect(QRect(0, 0, std::max(maxWidth - 2 * padding, 1), 2000),
                         Qt::TextWordWrap, m_callout);
  const QSize size(text.width() + 2 * padding, text.height() + 2 * padding);
  QRect bubble(QPoint(0, 0), size);

  const bool rtl = layoutDirection() == Qt::RightToLeft;

  if (target.isEmpty()) {
    // Nothing to point at (e.g. the native macOS menu bar): top centre.
    bubble.moveTop(area.top() + 4);
    bubble.moveLeft(area.center().x() - size.width() / 2);
  } else if (target.height() > rect().height() * 0.6) {
    // Big targets like the 3D view: put the label just inside the top.
    bubble.moveTop(target.top() + 16);
    bubble.moveLeft(target.center().x() - size.width() / 2);
  } else {
    const QRect ring = ringRect(target);
    // Try below, then above, then to the side. The side that fits is checked
    // against the ring so the bubble never covers the outline.
    const bool fitsBelow =
      ring.bottom() + arrow + size.height() <= area.bottom();
    const bool fitsAbove = ring.top() - arrow - size.height() >= area.top();
    const bool fitsRight = ring.right() + arrow + size.width() <= area.right();
    const bool fitsLeft = ring.left() - arrow - size.width() >= area.left();
    // On the side the text reads towards first, mirrored for right-to-left.
    const bool sideRight = rtl ? !fitsLeft && fitsRight : fitsRight;
    if (fitsBelow) {
      bubble.moveTop(ring.bottom() + arrow);
      bubble.moveLeft(ring.center().x() - size.width() / 2);
    } else if (fitsAbove) {
      bubble.moveBottom(ring.top() - arrow);
      bubble.moveLeft(ring.center().x() - size.width() / 2);
    } else if (sideRight) {
      bubble.moveLeft(ring.right() + arrow);
      bubble.moveTop(ring.center().y() - size.height() / 2);
    } else {
      bubble.moveRight(ring.left() - arrow);
      bubble.moveTop(ring.center().y() - size.height() / 2);
    }
  }

  // Keep the whole label inside the window.
  if (bubble.left() < area.left())
    bubble.moveLeft(area.left());
  if (bubble.right() > area.right())
    bubble.moveRight(area.right());
  if (bubble.top() < area.top())
    bubble.moveTop(area.top());
  if (bubble.bottom() > area.bottom())
    bubble.moveBottom(area.bottom());
  return bubble;
}

// A generous bound on what paintEvent() touches (ring, bubble, pointer and
// antialiasing), used to limit repaints to the changed region.
QRect HighlightOverlay::paintedArea() const
{
  QRect area = ringRect(m_target);
  const QRect bubble = calloutRect(m_target);
  if (!bubble.isEmpty())
    area = area.isEmpty() ? bubble : area.united(bubble);
  const int margin = QFontMetrics(font()).height() + 8;
  return area.adjusted(-margin, -margin, margin, margin);
}

void HighlightOverlay::paintEvent(QPaintEvent*)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Colours are read from the palette on every paint, so a theme change
  // (including high contrast) is picked up.
  const QPalette colors = palette();
  const QColor halo = colors.color(QPalette::Base);
  const QColor ringColor = colors.color(QPalette::Highlight);

  const qint64 elapsed = m_clock.isValid() ? m_clock.elapsed() : 0;

  // Draw the ring only when there is a target on screen; an empty target
  // (native menu bar, hidden widget) leaves just the callout.
  if (!m_target.isEmpty()) {
    double grow = kRingMargin;
    int ringWidth = kRingWidth;
    double alpha = 1.0;
    // Reduce motion: a steady ring, thicker for a short while on a reveal.
    // Otherwise the alpha pulses between 0.7 and 1.0, and a reveal starts
    // enlarged and settles over kRevealGrowMs.
    if (m_reduceMotion) {
      if (m_reveal && elapsed < kStaticRevealMs)
        ringWidth = kThickRingWidth;
    } else {
      const double phase =
        kTwoPi * std::fmod(elapsed, kPulsePeriodMs) / kPulsePeriodMs;
      alpha = 0.7 + 0.3 * (0.5 + 0.5 * std::sin(phase));
      if (m_reveal && elapsed < kRevealGrowMs)
        grow += kRevealGrowPixels * (1.0 - elapsed / kRevealGrowMs);
    }
    const QRectF ring = QRectF(m_target).adjusted(-grow, -grow, grow, grow);
    // Two-tone: a halo in the base colour, wider than the ring by
    // kHaloExtra on each side and fully opaque, under the ring in the
    // highlight colour. One of the two contrasts with any background.
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(halo, ringWidth + 2 * kHaloExtra));
    painter.drawRoundedRect(ring, 6, 6);
    QColor tone = ringColor;
    tone.setAlphaF(static_cast<float>(alpha));
    painter.setPen(QPen(tone, ringWidth));
    painter.drawRoundedRect(ring, 6, 6);
  }

  const QRect bubble = calloutRect(m_target);
  if (bubble.isEmpty())
    return;

  const QFontMetrics metrics(font());
  const int padding = std::max(6, metrics.height() / 2);
  const int arrow = std::max(8, metrics.height() * 2 / 3);

  QPainterPath path;
  path.addRoundedRect(QRectF(bubble), 6, 6);

  // A small pointer from the label toward the outline, on whichever side of
  // the ring the bubble was placed, unless the bubble sits on the target
  // itself (large targets such as the 3D view).
  if (!m_target.isEmpty() && !m_target.contains(bubble.center())) {
    const QRect ring = ringRect(m_target);
    QPolygonF pointer;
    if (bubble.top() >= ring.bottom()) {
      const int x =
        clampInt(ring.center().x(), bubble.left() + 12, bubble.right() - 12);
      pointer << QPointF(x - arrow, bubble.top() + 1)
              << QPointF(x, bubble.top() - arrow)
              << QPointF(x + arrow, bubble.top() + 1);
    } else if (bubble.bottom() <= ring.top()) {
      const int x =
        clampInt(ring.center().x(), bubble.left() + 12, bubble.right() - 12);
      pointer << QPointF(x - arrow, bubble.bottom() - 1)
              << QPointF(x, bubble.bottom() + arrow)
              << QPointF(x + arrow, bubble.bottom() - 1);
    } else if (bubble.left() >= ring.right()) {
      const int y =
        clampInt(ring.center().y(), bubble.top() + 12, bubble.bottom() - 12);
      pointer << QPointF(bubble.left() + 1, y - arrow)
              << QPointF(bubble.left() - arrow, y)
              << QPointF(bubble.left() + 1, y + arrow);
    } else if (bubble.right() <= ring.left()) {
      const int y =
        clampInt(ring.center().y(), bubble.top() + 12, bubble.bottom() - 12);
      pointer << QPointF(bubble.right() - 1, y - arrow)
              << QPointF(bubble.right() + arrow, y)
              << QPointF(bubble.right() - 1, y + arrow);
    }
    // The pointer is a triangle drawn overlapping the bubble's edge by a
    // pixel, then merged with the bubble so the outline is one shape.
    if (!pointer.isEmpty()) {
      QPainterPath tip;
      tip.addPolygon(pointer);
      tip.closeSubpath();
      path = path.united(tip);
    }
  }

  painter.setPen(QPen(ringColor, 2.0));
  painter.setBrush(colors.color(QPalette::ToolTipBase));
  painter.drawPath(path);

  // Text starts at the leading edge: left in left-to-right locales, right in
  // right-to-left ones.
  painter.setFont(font());
  painter.setPen(colors.color(QPalette::ToolTipText));
  painter.setLayoutDirection(layoutDirection());
  painter.drawText(bubble.adjusted(padding, padding, -padding, -padding),
                   QStyle::visualAlignment(
                     layoutDirection(), Qt::AlignLeading | Qt::AlignVCenter) |
                     Qt::TextWordWrap,
                   m_callout);
}

} // namespace Avogadro
