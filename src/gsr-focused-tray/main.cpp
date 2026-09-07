#include <KStatusNotifierItem>

#include <QAction>
#include <QApplication>
#include <QDBusConnection>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QProcess>
#include <QSocketNotifier>
#include <QString>

#include <sys/syscall.h>
#include <unistd.h>

class TrayController final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.odin.GsrTray")

public:
    explicit TrayController(QObject *parent = nullptr)
        : QObject(parent), tray_(new KStatusNotifierItem(QStringLiteral("gsr-focused-recorder"), this)) {
        tray_->setTitle(QStringLiteral("Focused Screen Recorder"));
        // Plasma may auto-hide SystemServices even while their status is Active.
        // This is a user-facing recording indicator and must remain visible.
        tray_->setCategory(KStatusNotifierItem::ApplicationStatus);
        tray_->setStatus(KStatusNotifierItem::Active);
        tray_->setStandardActionsEnabled(false);

        auto *menu = new QMenu;
        outputAction_ = menu->addAction(QStringLiteral("No active recording"));
        outputAction_->setEnabled(false);
        menu->addSeparator();

        auto *openAction = menu->addAction(QStringLiteral("Open recordings folder"));
        connect(openAction, &QAction::triggered, this, [] {
            const QString directory = QDir::homePath() + QStringLiteral("/Videos/GPUScreenRecorder");
            QProcess::startDetached(QStringLiteral("xdg-open"), {directory});
        });

        auto *quitAction = menu->addAction(QStringLiteral("Quit indicator"));
        connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
        tray_->setContextMenu(menu);

        stateDir_ = qEnvironmentVariable("XDG_STATE_HOME",
            QDir::homePath() + QStringLiteral("/.local/state")) +
            QStringLiteral("/gsr-focused-recorder");
        QFile pidFile(stateDir_ + QStringLiteral("/recorder.pid"));
        QFile outputFile(stateDir_ + QStringLiteral("/recorder.output"));
        bool active = false;
        QString output;
        qint64 pid = 0;
        if (pidFile.open(QIODevice::ReadOnly)) {
            bool ok = false;
            pid = QString::fromUtf8(pidFile.readAll()).trimmed().toLongLong(&ok);
            active = ok && QFile::exists(QStringLiteral("/proc/%1/cmdline").arg(pid));
        }
        if (active && outputFile.open(QIODevice::ReadOnly)) {
            output = QString::fromUtf8(outputFile.readAll()).trimmed();
        }
        setRecording(active, output);
        if (active) {
            watchPid(pid);
        }
    }

public slots:
    Q_SCRIPTABLE void SetRecording(bool recording, const QString &output, qlonglong pid) {
        setRecording(recording, output);
        if (recording) {
            watchPid(pid);
        } else {
            clearPidWatch();
        }
    }

private:
    void clearPidWatch() {
        if (pidNotifier_) {
            pidNotifier_->setEnabled(false);
            pidNotifier_->deleteLater();
            pidNotifier_ = nullptr;
        }
        if (pidfd_ >= 0) {
            close(pidfd_);
            pidfd_ = -1;
        }
    }

    void watchPid(qint64 pid) {
        clearPidWatch();
        if (pid <= 0) {
            return;
        }

        pidfd_ = static_cast<int>(syscall(SYS_pidfd_open, static_cast<pid_t>(pid), 0));
        if (pidfd_ < 0) {
            return;
        }

        pidNotifier_ = new QSocketNotifier(pidfd_, QSocketNotifier::Read, this);
        connect(pidNotifier_, &QSocketNotifier::activated, this, [this] {
            clearPidWatch();
            QFile::remove(stateDir_ + QStringLiteral("/recorder.pid"));
            QFile::remove(stateDir_ + QStringLiteral("/recorder.output"));
            QFile::remove(stateDir_ + QStringLiteral("/session-active"));
            setRecording(false, QString());
        });
    }

    void setRecording(bool recording, const QString &output) {
        if (recording) {
            tray_->setIconByName(QStringLiteral("media-record"));
            tray_->setToolTipIconByName(QStringLiteral("media-record"));
            tray_->setToolTipTitle(QStringLiteral("Recording"));
            tray_->setToolTipSubTitle(output.isEmpty() ? QStringLiteral("Focused application") : output);
            outputAction_->setText(output.isEmpty() ? QStringLiteral("Recording") : output);
        } else {
            tray_->setIconByName(QStringLiteral("media-playback-stop"));
            tray_->setToolTipIconByName(QStringLiteral("media-playback-stop"));
            tray_->setToolTipTitle(QStringLiteral("Not recording"));
            tray_->setToolTipSubTitle(QStringLiteral("Focused Screen Recorder"));
            outputAction_->setText(QStringLiteral("No active recording"));
        }
        tray_->setStatus(KStatusNotifierItem::Active);
    }

    KStatusNotifierItem *tray_;
    QAction *outputAction_;
    QString stateDir_;
    int pidfd_ = -1;
    QSocketNotifier *pidNotifier_ = nullptr;
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("gsr-focused-tray"));
    app.setQuitOnLastWindowClosed(false);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("org.odin.GsrTray"))) {
        return 0;
    }

    TrayController controller;
    if (!bus.registerObject(QStringLiteral("/Tray"), &controller,
                            QDBusConnection::ExportScriptableSlots)) {
        return 1;
    }

    return app.exec();
}

#include "main.moc"
