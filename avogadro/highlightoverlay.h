/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifndef AVOGADRO_HIGHLIGHTOVERLAY_H
#define AVOGADRO_HIGHLIGHTOVERLAY_H

#include <QtCore/QElapsedTimer>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtWidgets/QWidget>

#include <functional>

namespace Avogadro {

/**
 * @brief A see-through layer over a top-level window that outlines one part
 * of the interface and shows a text callout pointing at it.
 *
 * The outline is a two-tone ring (a halo in the base colour under a ring in
 * the highlight colour), so it shows on light, dark and high-contrast
 * palettes, and the callout always carries text, so the target is never
 * marked by colour alone. The overlay ignores the mouse and never takes
 * focus.
 */
class HighlightOverlay : public QWidget
{
  Q_OBJECT

public:
  /// Returns the target's rectangle in global coordinates, or an empty
  /// rectangle when there is nothing to outline (the callout is then shown
  /// at the top of the window).
  using Locator = std::function<QRect()>;

  explicit HighlightOverlay(QWidget* window);

  /**
   * Outline the target the locator reports, with @p callout beside it.
   * @param reveal Add the one-off enlargement ("Show me").
   * @param reduceMotion No pulse and no enlargement; a reveal gets a thicker
   * static ring for a couple of seconds instead.
   */
  void showTarget(const Locator& locator, const QString& callout, bool reveal,
                  bool reduceMotion);
  /// Hide the overlay and forget the target.
  void clearTarget();

protected:
  /// Watches the host window so the overlay follows its size.
  bool eventFilter(QObject* watched, QEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void changeEvent(QEvent* event) override;

private:
  /// Make the overlay cover the whole host window.
  void syncToWindow();
  /// Timer step: follow the target and repaint (the pulse).
  void tick();
  /// Ask the locator for the target and convert it to local coordinates.
  void relocate();
  /// The ring's largest extent: the target grown by the reveal enlargement.
  QRect ringRect(const QRect& target) const;
  /// Where the callout bubble goes (local coordinates), or empty if none.
  QRect calloutRect(const QRect& target) const;
  /// Everything that may be painted, for invalidating the right region.
  QRect paintedArea() const;

  Locator m_locator;
  QRect m_target; // in this widget's coordinates
  QString m_callout;
  QTimer m_timer; ///< Drives the pulse, or slow re-checks with reduce motion.
  QElapsedTimer m_clock; ///< Time since showTarget(), for the animation phase.
  bool m_reveal = false;
  bool m_reduceMotion = false;
};

} // namespace Avogadro

#endif // AVOGADRO_HIGHLIGHTOVERLAY_H
