#include <Purpose/AlternativesModel>
#include <Purpose/Menu>

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QTimer>
#include <QUrl>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("gsr-share-youtube"));
    app.setDesktopFileName(QStringLiteral("gsr-share-youtube"));
    app.setQuitOnLastWindowClosed(true);

    if (argc != 2) {
        return 2;
    }

    const QFileInfo file(QString::fromLocal8Bit(argv[1]));
    if (!file.exists() || !file.isFile()) {
        return 3;
    }

    const QUrl url = QUrl::fromLocalFile(file.absoluteFilePath());
    const QString mimeType = QMimeDatabase().mimeTypeForFile(file).name();

    Purpose::Menu menu;
    menu.model()->setPluginType(QStringLiteral("Export"));
    menu.model()->setInputData(QJsonObject{
        {QStringLiteral("mimeType"), mimeType},
        {QStringLiteral("urls"), QJsonArray{url.toString()}},
    });
    menu.reload();

    QObject::connect(&menu, &Purpose::Menu::finished, &app,
                     [&app](const QJsonObject &output, int error, const QString &) {
        if (error == 0 && output.contains(QStringLiteral("url"))) {
            QDesktopServices::openUrl(QUrl(output.value(QStringLiteral("url")).toString()));
        }
        app.exit(error == 0 ? 0 : 1);
    });

    QTimer::singleShot(0, &menu, [&menu, &app] {
        for (QAction *action : menu.actions()) {
            if (action->property("pluginId").toString() == QStringLiteral("youtubeplugin")) {
                action->trigger();
                return;
            }
        }
        app.exit(4);
    });

    return app.exec();
}
