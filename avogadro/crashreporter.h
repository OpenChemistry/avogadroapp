/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifndef AVOGADRO_CRASHREPORTER_H
#define AVOGADRO_CRASHREPORTER_H

#include "avogadroappconfig.h"

// QStringList is a type alias in Qt 6 rather than a class, so it cannot be
// forward declared by hand - this is Qt's own header for the purpose. It also
// declares QString.
#include <QtCore/qcontainerfwd.h>

class QOpenGLWidget;
class QWidget;

namespace Avogadro {

/**
 * @class CrashReporter crashreporter.h
 * @brief Optional crash reporting, backed by sentry-native.
 *
 * Only the separate Windows diagnostic build enables this. In every other
 * build each method below is an inline no-op, so callers never need to guard
 * their calls.
 *
 * Nothing is uploaded until the user gives consent. Until then reports are
 * written to a local database and either sent later, if consent is given, or
 * discarded.
 */
class CrashReporter
{
public:
  /** @return true if crash reporting was compiled in and a DSN is configured */
  static bool isAvailable();

  /**
   * Start the crash handler.
   *
   * Call this from main() before the QApplication instance is constructed, so
   * that crashes during Qt platform plugin initialization are also captured.
   * The organization and application names must already be set, since they
   * determine where the local crash database is kept.
   *
   * Prefer declaring a CrashReporterGuard over calling this directly.
   */
  static void initialize();

  /** Flush any pending reports and stop the crash handler. */
  static void shutdown();

  /** @return true if the user has agreed to upload crash reports */
  static bool hasConsent();

  /** @return true if the user has already been asked about crash reports */
  static bool consentRequested();

  /** Record the user's decision and apply it to the running handler. */
  static void setConsent(bool consentGiven);

  /**
   * Ask the user about crash reporting, if they have not been asked before.
   *
   * Call this once the main window is up. Does nothing on a build without
   * crash reporting, or once the question has been answered.
   */
  static void promptForConsentIfNeeded(QWidget* parentWidget);

  /** Show the crash reporting settings, letting the user change their mind. */
  static void showSettingsDialog(QWidget* parentWidget);

  /**
   * Gather what the crash reporter can learn about this session: the OpenGL
   * driver in use and which plugins are loaded.
   *
   * Call once, after the plugins have loaded and the view exists. The driver
   * strings are read on the first rendered frame, since there is no context
   * to query before then. @a glWidget may be null, in which case only the
   * plugin list is recorded.
   */
  static void attachSessionContext(QOpenGLWidget* glWidget);

  /**
   * Leave a trail of what was happening in the run-up to a crash.
   *
   * Pass only non-identifying values: a file format identifier rather than a
   * file name, a tool name rather than what it was applied to.
   *
   * Unlike the rest of this class, guard calls that have to build their
   * message with isAvailable() - formatting the string is not free, and would
   * otherwise be paid on every platform to be thrown away.
   */
  static void addBreadcrumb(const QString& category, const QString& message);

  /**
   * Deliberately crash the application to verify the crash handler.
   *
   * This exists only to validate the reporting pipeline end to end, and is
   * reachable solely through the --crash-test command line option in a build
   * configured with USE_SENTRY.
   */
  static void triggerTestCrash();
};

/**
 * @class CrashReporterGuard crashreporter.h
 * @brief Starts crash reporting on construction and stops it on destruction.
 *
 * Declaring one of these in main() keeps the handler running for the whole
 * lifetime of the process, including the early return paths.
 */
class CrashReporterGuard
{
public:
  CrashReporterGuard() { CrashReporter::initialize(); }
  ~CrashReporterGuard() { CrashReporter::shutdown(); }

  CrashReporterGuard(const CrashReporterGuard& other) = delete;
  CrashReporterGuard& operator=(const CrashReporterGuard& other) = delete;
};

#ifndef AVOGADRO_USE_SENTRY

// Builds without crash reporting get no object code at all - crashreporter.cpp
// is only compiled when USE_SENTRY is on.
inline bool CrashReporter::isAvailable()
{
  return false;
}
inline void CrashReporter::initialize() {}
inline void CrashReporter::shutdown() {}
inline bool CrashReporter::hasConsent()
{
  return false;
}
inline bool CrashReporter::consentRequested()
{
  return false;
}
inline void CrashReporter::setConsent(bool) {}
inline void CrashReporter::promptForConsentIfNeeded(QWidget*) {}
inline void CrashReporter::showSettingsDialog(QWidget*) {}
inline void CrashReporter::attachSessionContext(QOpenGLWidget*) {}
inline void CrashReporter::addBreadcrumb(const QString&, const QString&) {}
inline void CrashReporter::triggerTestCrash() {}

#endif // AVOGADRO_USE_SENTRY

} // end namespace Avogadro

#endif // AVOGADRO_CRASHREPORTER_H
