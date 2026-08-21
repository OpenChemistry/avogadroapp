/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

// This file is only compiled when the build is configured with USE_SENTRY.
// Builds without it use the inline no-op definitions in crashreporter.h.

#include "crashreporter.h"

#ifndef AVOGADRO_USE_SENTRY
// Without this the inline no-op definitions in the header are also in scope,
// and the failure is a wall of redefinition errors rather than a useful one.
#error "crashreporter.cpp requires a build configured with USE_SENTRY"
#endif

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>

#include <sentry.h>

#include <cctype>
#include <string>
#include <vector>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// Set by CMake, but fall back to something honest for a local build that
// forgot to pass one.
#ifndef AVOGADRO_SENTRY_ENVIRONMENT
#define AVOGADRO_SENTRY_ENVIRONMENT "development"
#endif

namespace Avogadro {

namespace {

const char* consentKey = "crashReporting/consent";
const char* consentAskedKey = "crashReporting/consentAsked";

bool sentryInitialized = false;

/// Lowercased spellings of the user's home directory, used to strip it out of
/// outgoing events. Computed once in initialize() because the before_send
/// callback can run inside an exception filter, where calling into Qt is not
/// safe. Both separator styles are kept: Qt reports '/' while the Windows APIs
/// and most driver messages report '\'.
std::vector<std::string> homeDirectoryNeedles;

constexpr char homeReplacement[] = "<user home>";
constexpr size_t homeReplacementLength = sizeof(homeReplacement) - 1;

char toLowerChar(char character)
{
  return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
}

std::string toLower(const std::string& text)
{
  std::string result(text);
  for (char& character : result)
    character = toLowerChar(character);
  return result;
}

/// Case insensitive search for @a needle within @a text, starting at @a from.
/// @a needle must already be lowercased. Allocates nothing, which matters
/// because this runs on the before_send path.
std::string::size_type findLowered(const std::string& text,
                                   const std::string& needle,
                                   std::string::size_type from)
{
  if (needle.empty() || text.size() < needle.size())
    return std::string::npos;

  const std::string::size_type last = text.size() - needle.size();
  for (std::string::size_type at = from; at <= last; ++at) {
    bool matched = true;
    for (std::string::size_type i = 0; i < needle.size(); ++i) {
      if (toLowerChar(text[at + i]) != needle[i]) {
        matched = false;
        break;
      }
    }
    if (matched)
      return at;
  }

  return std::string::npos;
}

/// Replace every spelling of the user's home directory in @a text.
/// @return true if @a text was modified.
bool scrubString(std::string& text)
{
  if (homeDirectoryNeedles.empty() || text.empty())
    return false;

  // Windows paths are case insensitive, so compare that way. The search
  // itself allocates nothing, so a string with no home directory in it - by
  // far the common case - costs only the scan.
  bool modified = false;

  for (const std::string& needle : homeDirectoryNeedles) {
    std::string::size_type at = findLowered(text, needle, 0);
    while (at != std::string::npos) {
      text.replace(at, needle.size(), homeReplacement);
      modified = true;
      at = findLowered(text, needle, at + homeReplacementLength);
    }
  }

  return modified;
}

/// Scrub the string stored at @a key of @a parent, if there is one.
void scrubValueAtKey(sentry_value_t parent, const char* key)
{
  sentry_value_t value = sentry_value_get_by_key(parent, key);
  if (sentry_value_get_type(value) != SENTRY_VALUE_TYPE_STRING)
    return;

  const char* raw = sentry_value_as_string(value);
  if (raw == nullptr)
    return;

  std::string text(raw);
  if (!scrubString(text))
    return;

  sentry_value_set_by_key(parent, key, sentry_value_new_string(text.c_str()));
}

/// Strip the user's home directory out of the fields we populate.
///
/// Note the limits of this, which are a property of the crashpad backend
/// rather than of this function: for an actual crash the event handed to
/// before_send is empty, because the exception and stack trace are rebuilt
/// server side from the minidump. So this only cleans non-crash events and the
/// breadcrumbs riding along with them. Anything the minidump itself contains
/// has to be handled by the server side scrubbing rules on the Sentry project.
sentry_value_t scrubEvent(sentry_value_t event, void* hint, void* userData)
{
  Q_UNUSED(hint);
  Q_UNUSED(userData);

  // Breadcrumbs are where user paths actually turn up, since the Qt
  // integration turns every qWarning into one.
  sentry_value_t breadcrumbs = sentry_value_get_by_key(event, "breadcrumbs");
  sentry_value_t values = sentry_value_get_by_key(breadcrumbs, "values");
  if (sentry_value_get_type(values) != SENTRY_VALUE_TYPE_LIST)
    values = breadcrumbs;

  if (sentry_value_get_type(values) == SENTRY_VALUE_TYPE_LIST) {
    const size_t count = sentry_value_get_length(values);
    for (size_t i = 0; i < count; ++i)
      scrubValueAtKey(sentry_value_get_by_index(values, i), "message");
  }

  scrubValueAtKey(event, "message");

  sentry_value_t logentry = sentry_value_get_by_key(event, "logentry");
  if (sentry_value_get_type(logentry) == SENTRY_VALUE_TYPE_OBJECT)
    scrubValueAtKey(logentry, "message");

  return event;
}

void rememberHomeDirectory()
{
  homeDirectoryNeedles.clear();

  const QString home = QDir::homePath();
  if (home.isEmpty())
    return;

  const QString slashes = QDir::fromNativeSeparators(home);
  const QString backslashes = QDir::toNativeSeparators(home);

  homeDirectoryNeedles.push_back(toLower(slashes.toStdString()));
  if (backslashes != slashes)
    homeDirectoryNeedles.push_back(toLower(backslashes.toStdString()));
}

/// Absolute path to the crashpad handler, which lives next to our executable.
///
/// This deliberately avoids QCoreApplication::applicationDirPath(), because
/// the crash handler is started before the QApplication instance exists.
QString crashHandlerPath()
{
#ifdef Q_OS_WIN
  std::vector<wchar_t> buffer(MAX_PATH);
  for (;;) {
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0)
      return QString();
    // On truncation the call returns the buffer size it was given.
    if (static_cast<size_t>(length) < buffer.size())
      break;
    buffer.resize(buffer.size() * 2);
  }

  const QString executable = QString::fromWCharArray(buffer.data());
  return QFileInfo(executable)
    .absoluteDir()
    .absoluteFilePath("crashpad_handler.exe");
#else
  // Crash reporting is Windows only for now.
  return QString();
#endif
}

void applyConsent(bool consentGiven)
{
  if (!sentryInitialized)
    return;

  if (consentGiven)
    sentry_user_consent_give();
  else
    sentry_user_consent_revoke();
}

} // end anonymous namespace

bool CrashReporter::isAvailable()
{
#ifdef AVOGADRO_SENTRY_DSN
  return true;
#else
  // Built with USE_SENTRY but no DSN was configured, so there is nowhere to
  // send anything.
  return false;
#endif
}

void CrashReporter::initialize()
{
#ifdef AVOGADRO_SENTRY_DSN
  if (sentryInitialized || !isAvailable())
    return;

  const QString dataLocation =
    QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  if (dataLocation.isEmpty()) {
    qWarning("Crash reporting disabled: no writable data location.");
    return;
  }

  const QString databasePath =
    QDir(dataLocation).absoluteFilePath("crashpad-db");
  QDir().mkpath(databasePath);

  const QString handlerPath = crashHandlerPath();
  if (handlerPath.isEmpty() || !QFileInfo::exists(handlerPath)) {
    qWarning("Crash reporting disabled: crashpad handler was not found.");
    return;
  }

  rememberHomeDirectory();

  sentry_options_t* options = sentry_options_new();
  sentry_options_set_dsn(options, AVOGADRO_SENTRY_DSN);

#ifdef Q_OS_WIN
  // The wide character setters matter here: Windows profile paths regularly
  // contain non-ASCII characters, and the narrow variants would mangle them,
  // leaving the handler silently unable to write its database.
  const std::wstring database =
    QDir::toNativeSeparators(databasePath).toStdWString();
  const std::wstring handler =
    QDir::toNativeSeparators(handlerPath).toStdWString();
  sentry_options_set_database_pathw(options, database.c_str());
  sentry_options_set_handler_pathw(options, handler.c_str());
#else
  sentry_options_set_database_path(options, databasePath.toUtf8().constData());
  sentry_options_set_handler_path(options, handlerPath.toUtf8().constData());
#endif

  // Must match the release that sentry-cli uploads debug symbols under, or
  // nothing will symbolicate.
  sentry_options_set_release(options, "avogadro2@" AvogadroApp_VERSION);
  sentry_options_set_environment(options, AVOGADRO_SENTRY_ENVIRONMENT);

  // Reports are collected locally but held back until consent is given.
  sentry_options_set_require_user_consent(options, 1);
  sentry_options_set_max_breadcrumbs(options, 50);
  sentry_options_set_before_send(options, scrubEvent, nullptr);

  if (sentry_init(options) != 0) {
    qWarning("Crash reporting could not be started.");
    return;
  }

  sentryInitialized = true;

  // Carry over the decision the user made in an earlier run.
  applyConsent(hasConsent());
#endif
}

void CrashReporter::shutdown()
{
  if (!sentryInitialized)
    return;

  sentryInitialized = false;
  sentry_close();
}

bool CrashReporter::hasConsent()
{
  if (!isAvailable())
    return false;

  QSettings settings;
  return settings.value(consentKey, false).toBool();
}

bool CrashReporter::consentRequested()
{
  if (!isAvailable())
    return false;

  QSettings settings;
  return settings.value(consentAskedKey, false).toBool();
}

void CrashReporter::setConsent(bool consentGiven)
{
  if (!isAvailable())
    return;

  QSettings settings;
  settings.setValue(consentKey, consentGiven);
  settings.setValue(consentAskedKey, true);

  applyConsent(consentGiven);
}

void CrashReporter::triggerTestCrash()
{
  // Deliberate null dereference. This is the one place in the application
  // that is meant to crash: it exists to prove the handler, the symbols and
  // the upload path all work, and is only reachable via --crash-test.
  volatile int* pointer = nullptr;
  *pointer = 0;
}

} // end namespace Avogadro
