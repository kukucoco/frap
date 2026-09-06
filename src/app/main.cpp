/*
 * frap 主程序（QtQuick 覆盖层）
 *
 * 启动流程：
 *   1. 加载设置 + 启动插件宿主
 *   2. 通过 freedesktop portal 触发系统截图
 *   3. 就绪后创建 QQuickView（LayerShell 覆盖层）加载 qml/main.qml
 *      - 交互状态机在 QML（CaptureController），原语在 C++（FrapBackend）
 */
#include <QApplication>
#include <QQuickView>
#include <QQmlContext>
#include <QQmlEngine>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusObjectPath>
#include <QDebug>
#include <QImage>
#include <QDir>
#include <QTimer>
#include <QScreen>
#include <QStandardPaths>
#include <LayerShellQt/Window>

#include "settings.h"
#include "plugin_manager.h"
#include "session.h"
#include "qml_backend.h"
#include "frame_image.h"
#include "icon_provider.h"
#include "logging.h"
#include "crash_handler.h"

using namespace sc;

/* 通过 DBus 调用 freedesktop portal 的 Screenshot 接口 */
class PortalCapture : public QObject
{
    Q_OBJECT
public:
    explicit PortalCapture(QObject* parent = nullptr) : QObject(parent) {}

    void request()
    {
        auto bus = QDBusConnection::sessionBus();
        QDBusMessage msg = QDBusMessage::createMethodCall(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.Screenshot",
            "Screenshot");
        QVariantMap options;
        options["interactive"] = false;
        options["handle_token"] = "myscr_001";
        msg.setArguments({ QString(""), options });

        QDBusPendingCall pending = bus.asyncCall(msg);
        auto* watcher = new QDBusPendingCallWatcher(pending, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, bus](QDBusPendingCallWatcher* w) {
            w->deleteLater();
            QDBusPendingReply<QDBusObjectPath> reply = *w;
            if (reply.isError()) {
                qWarning() << "portal Screenshot 调用失败:" << reply.error().message();
                emit failed(reply.error().message());
                return;
            }
            const QDBusObjectPath reqPath = reply.value();
            QDBusConnection localBus = bus;
            bool ok = localBus.connect("org.freedesktop.portal.Desktop",
                                  reqPath.path(),
                                  "org.freedesktop.portal.Request",
                                  "Response",
                                  this,
                                  SLOT(handleResponse(uint, QVariantMap)));
            if (!ok)
                emit failed(localBus.lastError().message());
        });
    }

public slots:
    void handleResponse(uint responseCode, const QVariantMap& results)
    {
        if (responseCode != 0) {
            emit failed(QStringLiteral("用户取消或系统拒绝截图"));
            return;
        }
        const QUrl url(results.value("uri").toString());
        const QString filePath = url.toLocalFile();
        QImage snapshot;
        if (!snapshot.load(filePath)) {
            emit failed(QStringLiteral("无法读取截图文件: %1").arg(filePath));
            return;
        }
        emit captured(snapshot);
    }

signals:
    void captured(const QImage& image);
    void failed(const QString& message);
};

int main(int argc, char* argv[])
{
    sc::installConsoleLogging();
    sc::installCrashHandlers(QStringLiteral("frap"));
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("frap"));
    app.setOrganizationName(QStringLiteral("frap"));
    app.setApplicationVersion(QStringLiteral("0.2.0"));

    /* 设置 */
    Settings settings;
    const QString configRoot =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/frap");
    QDir().mkpath(configRoot);
    settings.load(configRoot + QStringLiteral("/settings.json"));

    /* 插件宿主 */
    PluginManager pluginManager(&settings);
    QObject::connect(&pluginManager, &PluginManager::pluginLog,
                     [](const QString& level, const QString& msg) {
                         qInfo() << "[host/" << level << "]" << msg;
                     });
    QObject::connect(&pluginManager, &PluginManager::failed,
                     [](const QString& message) {
                         qWarning() << "插件宿主不可用:" << message;
                     });
    pluginManager.start();

    /* 会话 + QML 后端 */
    CaptureSession session;
    FrapBackend backend(&session, &settings, &pluginManager);

    qmlRegisterType<sc::FrameImage>("Frap", 1, 0, "FrameImage");

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.engine()->addImportPath(QStringLiteral("qrc:/"));
    view.engine()->addImageProvider(QStringLiteral("icons"), new sc::IconProvider);
    view.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    view.setSource(QUrl(QStringLiteral("qrc:/Frap/main.qml")));
    if (view.status() != QQuickView::Ready)
        qWarning() << "QML 加载失败:" << view.errors();

    /* LayerShell 覆盖层 */
    auto* layerShell = LayerShellQt::Window::get(&view);
    if (layerShell) {
        layerShell->setLayer(LayerShellQt::Window::Layer::LayerOverlay);
        layerShell->setAnchors(LayerShellQt::Window::Anchors(
            LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight |
            LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom));
        layerShell->setMargins(QMargins(0, 0, 0, 0));
        layerShell->setKeyboardInteractivity(
            LayerShellQt::Window::KeyboardInteractivity::KeyboardInteractivityOnDemand);
        layerShell->setExclusiveZone(-1);
    } else {
        qWarning() << "未获取到 LayerShellQt（可能非 Wayland）";
    }
    if (QScreen* screen = QGuiApplication::primaryScreen())
        view.setGeometry(screen->geometry());

    QObject::connect(&backend, &FrapBackend::sessionFinished,
                     &app, &QCoreApplication::quit);

    /* 系统截图 */
    PortalCapture capture;
    bool capturingForLongshot = false;
    QObject::connect(&capture, &PortalCapture::failed, &app, [](const QString& message) {
        qWarning().noquote() << message;
        qApp->exit(1);
    });
    QObject::connect(&capture, &PortalCapture::captured, &app, [&](const QImage& image) {
        if (capturingForLongshot) {
            capturingForLongshot = false;
            backend.longshotAdd(image);
            return;
        }
        qInfo() << "portal 截图就绪:" << image.size();
        session.setImage(image);
        if (QScreen* screen = QGuiApplication::primaryScreen())
            session.setScreenSize(screen->geometry().size());
        view.show();
        view.requestActivate();
    });
    /* 长截图：请求再截一屏 */
    QObject::connect(&backend, &FrapBackend::needCapture, &app, [&]() {
        capturingForLongshot = true;
        capture.request();
    });

    QTimer::singleShot(0, &capture, [&capture]() { capture.request(); });

    return app.exec();
}

#include "main.moc"
