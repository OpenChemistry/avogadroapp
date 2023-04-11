#ifndef AUTOSAVE_H
#define AUTOSAVE_H

/************************************* Includes *************************************/
#include <QtWidgets>

#define AUTOSAVE_INTERVAL           300000 /* 3 minutes */
//#define DEBUG                            /* Uncomment for Debug messages */

/*************************************** Globals ***************************************/
class QTimer;

/******************************** Class Declaration *********************************/
class Autosave : public QObject
{
    Q_OBJECT

public:

    Autosave(QObject *parent);

    QTimer *timer;

    void timerStart();

    void createFile();

    void clearDirectory();

    bool fileExists();

    void setProjectName(QString name);

    void fileRecovery();

    QString getFilePath(void);

private:

    QString filepath, project_name;

    QString autosave_path;

    QDir autosave_dir;
};

#endif // AUTOSAVE_H