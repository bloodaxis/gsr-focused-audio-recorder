/*
    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <QApplication>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QLocalServer>
#include <QMenu>
#include <QPainter>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QThread>
#include <algorithm>
#include <deque>

// Keep rate sampling independent of network callbacks so a stalled transfer
// actually decays to zero instead of displaying its last successful speed.
class TransferRate
{
public:
    double update(qint64 now, qint64 bytes)
    {
        if (!samples.empty() && bytes < samples.back().second) {
            samples.clear();
        }
        samples.emplace_back(now, bytes);
        while (samples.size() > 2 && samples[1].first <= now - 3000) {
            samples.pop_front();
        }
        const auto [then, before] = samples.front();
        return now > then ? double(bytes - before) * 1000.0 / double(now - then) : 0.0;
    }
private:
    std::deque<std::pair<qint64, qint64>> samples;
};

static bool validVideoUrl(const QUrl &url)
{
    return url.scheme() == QStringLiteral("https") && url.host() == QStringLiteral("www.youtube.com")
        && url.path() == QStringLiteral("/watch") && url.userInfo().isEmpty();
}

static QString sizeText(double bytes)
{
    const QStringList units{QStringLiteral("B"), QStringLiteral("KiB"), QStringLiteral("MiB"), QStringLiteral("GiB")};
    int unit = 0;
    while (bytes >= 1024 && unit < units.size() - 1) {
        bytes /= 1024;
        ++unit;
    }
    return QStringLiteral("%1 %2").arg(bytes, 0, 'f', unit ? 1 : 0).arg(units[unit]);
}

static QIcon progressIcon(double fraction, bool complete, bool failed, int phase)
{
    QIcon icon;
    for (const int size : {22, 32, 48, 64}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(size / 32.0, size / 32.0);
        const QRectF ring(3, 3, 26, 26);
        painter.setPen(QPen(QColor(100, 100, 100, 190), 4));
        painter.drawEllipse(ring);
        painter.setPen(QPen(failed ? QColor("#ed5252") : complete ? QColor("#42cc78") : QColor("#3daee9"), 4, Qt::SolidLine, Qt::RoundCap));
        if (failed || complete) {
            painter.drawEllipse(ring);
        } else if (fraction < 0) {
            painter.drawArc(ring, phase * 16, 100 * 16);
        } else {
            painter.drawArc(ring, 90 * 16, -int(std::clamp(fraction, 0.0, 1.0) * 360 * 16));
        }
        painter.setPen(QPen(Qt::white, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (failed) {
            painter.drawLine(QPointF(16, 10), QPointF(16, 17));
            painter.drawPoint(QPointF(16, 22));
        } else if (complete) {
            painter.drawLine(QPointF(10, 16), QPointF(14, 20));
            painter.drawLine(QPointF(14, 20), QPointF(22, 12));
        } else {
            painter.drawLine(QPointF(16, 22), QPointF(16, 10));
            painter.drawLine(QPointF(16, 10), QPointF(11, 15));
            painter.drawLine(QPointF(16, 10), QPointF(21, 15));
        }
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

class UploadTray : public QObject
{
public:
    QString toolTip() const { return tray.toolTip(); }
    bool isVisible() const { return tray.isVisible(); }
    bool canOpenVideo() const { return validVideoUrl(videoUrl); }
    explicit UploadTray(const QString &socketName)
    {
        clock.start();
        openAction = menu.addAction(tr("Open YouTube Studio"), this, [this] { openDestination(); });
        menu.addSeparator();
        menu.addAction(tr("Dismiss indicator"), qApp, &QApplication::quit);
        tray.setContextMenu(&menu);
        connect(&tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
                openDestination();
            }
        });
        connect(&tray, &QSystemTrayIcon::messageClicked, this, [this] { openDestination(); });
        connect(&socket, &QLocalSocket::readyRead, this, [this] { readUpdates(); });
        connect(&socket, &QLocalSocket::disconnected, this, [this] {
            readUpdates();
            lostConnection();
        });
        connect(&socket, &QLocalSocket::errorOccurred, this, [this] { lostConnection(); });
        connect(&timer, &QTimer::timeout, this, [this] { refresh(); });
        timer.start(500);
        refresh();
        tray.show();
        socket.connectToServer(socketName, QIODevice::ReadOnly);
    }

private:
    bool terminal() const { return state == QStringLiteral("complete") || state == QStringLiteral("error"); }

    void openDestination()
    {
        // Plasma can deliver two activations for a double-click.
        if (state == QStringLiteral("error") || (lastOpen >= 0 && clock.elapsed() - lastOpen <= 1000)) {
            return;
        }
        lastOpen = clock.elapsed();
        if (state == QStringLiteral("complete")) {
            if (validVideoUrl(videoUrl) && QDesktopServices::openUrl(videoUrl)) {
                tray.hide();
                QApplication::quit();
            }
        } else {
            QDesktopServices::openUrl(QUrl(QStringLiteral("https://studio.youtube.com/")));
        }
    }

    void lostConnection()
    {
        if (!terminal()) {
            state = QStringLiteral("error");
            detail = tr("Lost contact with the uploader; completion could not be confirmed.");
            refresh();
            timer.stop();
        }
    }

    void readUpdates()
    {
        while (socket.canReadLine()) {
            const auto object = QJsonDocument::fromJson(socket.readLine()).object();
            if (object.isEmpty() || terminal()) {
                continue;
            }
            state = object.value(QStringLiteral("state")).toString();
            name = object.value(QStringLiteral("name")).toString();
            sent = object.value(QStringLiteral("sent")).toInteger();
            total = object.value(QStringLiteral("total")).toInteger();
            detail = object.value(QStringLiteral("detail")).toString();
            if (state == QStringLiteral("complete")) {
                videoUrl = QUrl(detail);
                lastOpen = -1;
                openAction->setText(tr("Open YouTube video"));
                openAction->setEnabled(validVideoUrl(videoUrl));
                tray.showMessage(tr("YouTube upload complete"), name, QSystemTrayIcon::Information);
            } else if (state == QStringLiteral("error")) {
                openAction->setEnabled(false);
                tray.showMessage(tr("YouTube upload failed"), detail, QSystemTrayIcon::Warning);
            }
        }
        refresh();
        if (terminal()) timer.stop();
    }

    void refresh()
    {
        const bool complete = state == QStringLiteral("complete");
        const bool failed = state == QStringLiteral("error");
        const double fraction = total > 0 ? double(sent) / double(total) : -1;
        const double speed = rate.update(clock.elapsed(), sent);
        QString status;
        if (complete) {
            status = tr("Upload complete — click to open video");
        } else if (failed) {
            status = detail;
        } else if (state == QStringLiteral("waiting")) {
            status = tr("100% sent — waiting for YouTube confirmation");
        } else if (state == QStringLiteral("uploading")) {
            status = tr("%1% · %2/s\n%3 / %4").arg(int(std::clamp(fraction, 0.0, 1.0) * 100))
                .arg(sizeText(speed), sizeText(sent), sizeText(total));
        } else {
            status = tr("Preparing upload…");
        }
        tray.setToolTip(tr("YouTube: %1\n%2").arg(name, status));
        tray.setIcon(progressIcon(fraction, complete, failed, int(clock.elapsed() / 10 % 360)));
    }

    QSystemTrayIcon tray;
    QMenu menu;
    QAction *openAction;
    QLocalSocket socket;
    QTimer timer;
    QElapsedTimer clock;
    TransferRate rate;
    QString state = QStringLiteral("preparing"), name, detail;
    QUrl videoUrl;
    qint64 sent = 0, total = 0, lastOpen = -1;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("purpose-youtube-tray"));
    app.setQuitOnLastWindowClosed(false);
    const QStringList args = app.arguments();
    if (args.size() == 2 && args[1] == QStringLiteral("--self-test")) {
        TransferRate rate;
        if (rate.update(0, 0) != 0 || rate.update(1000, 2048) != 2048) return 1;
        rate.update(2000, 2048);
        rate.update(3000, 2048);
        if (rate.update(4000, 2048) != 0 || rate.update(5000, 0) != 0) return 2;
        for (double fraction : {-1.0, 0.0, 0.5, 1.0}) {
            if (progressIcon(fraction, false, false, 0).isNull()) return 4;
        }
        // Exercise the real IPC path without Google credentials or an upload.
        QLocalServer server;
        if (!server.listen(QStringLiteral("youtube-tray-test-%1").arg(QUuid::createUuid().toString(QUuid::Id128)))) return 5;
        UploadTray tray(server.fullServerName());
        auto spinUntil = [](auto predicate) {
            QElapsedTimer deadline;
            deadline.start();
            while (!predicate() && deadline.elapsed() < 2000) {
                QApplication::processEvents();
                QThread::msleep(5);
            }
            return predicate();
        };
        if (!spinUntil([&] { return server.hasPendingConnections(); })) return 6;
        auto peer = server.nextPendingConnection();
        auto send = [&](const QString &state, const QString &detail = QString()) {
            peer->write(QJsonDocument(QJsonObject{{QStringLiteral("state"), state},
                {QStringLiteral("name"), QStringLiteral("test.mp4")},
                {QStringLiteral("sent"), 512}, {QStringLiteral("total"), 1024},
                {QStringLiteral("detail"), detail}}).toJson(QJsonDocument::Compact) + '\n');
            peer->flush();
        };
        send(QStringLiteral("uploading"));
        if (!spinUntil([&] { return tray.toolTip().contains(QStringLiteral("50%")); }) || !tray.isVisible()) return 7;
        send(QStringLiteral("waiting"));
        if (!spinUntil([&] { return tray.toolTip().contains(QStringLiteral("confirmation")); })) return 8;
        send(QStringLiteral("complete"), QStringLiteral("https://www.youtube.com/watch?v=test"));
        if (!spinUntil([&] { return tray.canOpenVideo(); }) || !tray.isVisible()) return 9;
        peer->disconnectFromServer();
        QApplication::processEvents();
        if (!tray.toolTip().contains(QStringLiteral("Upload complete"))) return 10;
        UploadTray interrupted(server.fullServerName());
        if (!spinUntil([&] { return server.hasPendingConnections(); })) return 11;
        auto brokenPeer = server.nextPendingConnection();
        brokenPeer->disconnectFromServer();
        if (!spinUntil([&] { return interrupted.toolTip().contains(QStringLiteral("Lost contact")); })) return 12;
        return 0;
    }
    if (args.size() != 3 || args[1] != QStringLiteral("--socket")) return 1;
    UploadTray tray(args[2]);
    return app.exec();
}
