/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "backgroundfileformat.h"

#include <avogadro/io/fileformat.h>

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTextStream>

namespace Avogadro {

BackgroundFileFormat::BackgroundFileFormat(Io::FileFormat* format,
                                           QObject* aparent)
  : QObject(aparent)
  , m_format(format)
  , m_molecule(nullptr)
  , m_success(false)
{
}

BackgroundFileFormat::~BackgroundFileFormat()
{
  delete m_format;
}

void BackgroundFileFormat::read()
{
  m_success = false;
  m_error.clear();

  if (!m_molecule)
    m_error = tr("No molecule set in BackgroundFileFormat!");

  if (!m_format)
    m_error = tr("No file format set in BackgroundFileFormat!");

  if (m_fileName.isEmpty())
    m_error = tr("No file name set in BackgroundFileFormat!");

  if (m_error.isEmpty()) {
    // sometimes we hit UTF-16 files, so we need to convert them to UTF-8
    // first check whether the file is UTF-16
    QFile file(m_fileName);
    QTextStream in(&file);
    QString text;
    bool isUTF16 = false;
    if (file.open(QIODevice::ReadOnly)) {
      QByteArray data = file.read(2);
      // look for a byte-order mark
      if ((data.size() == 2 && data[0] == '\xff' && data[1] == '\xfe') ||
          (data.size() == 2 && data[0] == '\xfe' && data[1] == '\xff')) {
        // UTF-16, read the file and let QString handle decoding
        isUTF16 = true;
        file.close();
        file.open(QIODevice::ReadOnly | QIODevice::Text);
#if QT_VERSION < 0x060000
        in.setCodec("UTF-16");
#endif
        text = in.readAll();
        file.close();
      }
    }

    if (!isUTF16)
      m_success =
        m_format->readFile(m_fileName.toLocal8Bit().data(), *m_molecule);
    else {
      // write it to a temporary file and we'll read it back in
      // some formats (like the generic output) need a file
      // not just a string bugger

      // first, we need the *name* of the file, not the full path
      // because we're going to save a copy in a temp directory
      QTemporaryDir tempDir;
      QString tempFileName = tempDir.filePath(QFileInfo(m_fileName).fileName());
      QFile tempFile(tempFileName);
      if (tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&tempFile);
        // set it to UTF-8
#if QT_VERSION > 0x060000
        out.setEncoding(QStringConverter::Utf8);
#else
        out.setCodec("UTF-8");
#endif
        out << text;
        out.flush();
        tempFile.close();
        m_success =
          m_format->readFile(tempFileName.toLocal8Bit().data(), *m_molecule);
        tempFile.remove();
      } else // try just reading the string
        m_success =
          m_format->readString(text.toLocal8Bit().data(), *m_molecule);
    }

    if (!m_success)
      m_error = QString::fromStdString(m_format->error());
  }

  emit finished();
}

void BackgroundFileFormat::write()
{
  m_success = false;
  m_error.clear();

  if (!m_molecule)
    m_error = tr("No molecule set in BackgroundFileFormat!");

  if (!m_format)
    m_error = tr("No file format set in BackgroundFileFormat!");

  if (m_fileName.isEmpty())
    m_error = tr("No file name set in BackgroundFileFormat!");

  if (m_error.isEmpty()) {
    m_success =
      m_format->writeFile(m_fileName.toLocal8Bit().data(), *m_molecule);

    if (!m_success)
      m_error = QString::fromStdString(m_format->error());
  }

  emit finished();
}

} // namespace Avogadro
