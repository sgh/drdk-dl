#include "drdk-dl.h"
#include "json.h"
#include "http.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>

class QtHttp : public IHttp {
	QNetworkAccessManager _netman;
public:
	std::string get(std::string url) override {
		auto request = QNetworkRequest(QUrl(url.c_str()));
		request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
		auto reply = _netman.get(request);
		QEventLoop loop;
		QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
		loop.exec();
		auto str = reply->readAll().toStdString();
 		return str;
	}

	std::string getToFile(std::string url, FILE *file, std::function<void (unsigned int)> fileposfunc) override {
		auto request = QNetworkRequest(QUrl(url.c_str()));
		request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
		auto reply = _netman.get(request);
		QEventLoop loop;
		QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
		loop.exec();
		auto str = reply->readAll().toStdString();
		fwrite(str.c_str(), str.size(), 1, file);
		fileposfunc(ftell(file)/1024);
 		return str;
	}
};


void extract_targets(struct video_meta& meta, const std::string& page) {
	auto doc = QJsonDocument::fromJson(page.c_str());
	auto links = doc.object()["Links"].toArray();
	foreach (QJsonValue v, links) {
		auto target = v.toObject()["Target"].toString();
		auto url = v.toObject()["Uri"].toString().toStdString();
		if (url.length() == 0)
			url = decrypt_uri(v.toObject()["EncryptedUri"].toString().toStdString());
		meta.uri[target.toStdString()] = url;
	}
}


std::unique_ptr<IHttp> createHttp() {
	return std::unique_ptr<IHttp>(new QtHttp);
}
