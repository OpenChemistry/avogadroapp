#ifndef IMPORTPQR_H
#define IMPORTPQR_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QNetworkRequest>
#include <QVariantMap>
#include <QTextBrowser>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLineEdit>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QPixmap>
#include <QDateTime>
#include <QFile>
#include <QIcon>
#include <QSize>
#include <QDir>
#include <QLabel>
#include <QtSvg/QGraphicsSvgItem>
#include <QProgressBar>
#include <cctype>
#include "json.h"

namespace Avogadro {

class ImportPQR : public QObject {
	Q_OBJECT
public:
	ImportPQR();
  void sendRequest(QString, QTableWidget*);
	void sendRequest(QString, QString, QString, QString);
	QString getMol2Url(int);
	void updateSVGPreview(QString, QString, QGraphicsView*);
private slots:
  void parseJson();
	void getFile();
	void setSVG();

private:
	QNetworkReply *reply;
	Json::Reader *read;
	Json::Value root;
  QNetworkAccessManager *oNetworkAccessManager;
  QVariantMap m_jsonResult;
	QProgressBar* progress;
	QGraphicsView* svgPreview;
	QPixmap svgImage;
	QGraphicsScene* svgScene;
	QTableWidget* table;
	QString currentFilename;
	QString currentDownloadFolder;
	struct result {
		QString inchikey;
		QString name;
		QString mol2url;
		QString formula;
		float mass;
		//useless?
		QString last_updated;
		QString *tags;
		QString *synonyms;
	};

	QString resultToString(result);
	float getMolMass(QString);
	float elementToMass(std::string);
	QString parseSubscripts(QString);
	result *results;
};
}
#endif // REQUEST_H
