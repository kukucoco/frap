/*
 * ui_preview —— 离线渲染覆盖层 UI 到 preview.png（验收/迭代 UI 用）
 *
 * 用法: ui_preview <插件目录> [输出.png]
 */
#include <QGuiApplication>
#include <QQuickView>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTimer>
#include <QImage>
#include <QDir>
#include <QStandardPaths>
#include <QElapsedTimer>
#include <QDebug>
#include <QPainter>

#include "settings.h"
#include "plugin_manager.h"
#include "session.h"
#include "qml_backend.h"
#include "frame_image.h"
#include "icon_provider.h"
#include "logging.h"

using namespace sc;

int main(int argc, char* argv[])
{
    installConsoleLogging();
    QGuiApplication app(argc, argv);
    qputenv("QT_QPA_PLATFORM", "offscreen");

    const QString pluginDir = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                       : QStringLiteral("plugins");
    const QString outPath = argc > 2 ? QString::fromLocal8Bit(argv[2])
                                     : QStringLiteral("preview.png");

    const QString configRoot = QDir::tempPath() + QStringLiteral("/myscr_preview_config");
    QDir().mkpath(configRoot);
    qputenv("XDG_CONFIG_HOME", configRoot.toUtf8());

    Settings settings;
    settings.setPluginDirectories({ pluginDir });

    CaptureSession session;
    PluginManager manager(&settings);
    manager.start();
    FrapBackend backend(&session, &settings, &manager);

    /* 预置画面 + 标注（后端已连接，触发帧渲染） */
    QImage image(1600, 900, QImage::Format_ARGB32);
    QPainter p(&image);
    p.fillRect(image.rect(), QColor("#2b2f38"));
    for (int i = 0; i < 8; ++i) {
        const QColor c(QColor::fromHsl((i * 45) % 360, 60, 50));
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(40 + i * 200, 60, 160, 140, 12, 12);
    }
    p.end();
    session.setImage(image);
    session.setScreenSize(QSizeF(1600, 900));
    /* 预置若干标注展示效果 */
    const auto rect = [](int x, int y, int w, int h) {
        return QJsonObject{
            { QStringLiteral("x"), x }, { QStringLiteral("y"), y },
            { QStringLiteral("w"), w }, { QStringLiteral("h"), h } };
    };
    const auto pt = [](int x, int y) {
        return QJsonObject{
            { QStringLiteral("x"), x }, { QStringLiteral("y"), y } };
    };
    session.addOperation(QJsonObject{
        { QStringLiteral("type"), QStringLiteral("rect") },
        { QStringLiteral("rect"), rect(200, 300, 300, 180) },
        { QStringLiteral("color"), QStringLiteral("#e11d48") },
        { QStringLiteral("width"), 3 } });
    session.addOperation(QJsonObject{
        { QStringLiteral("type"), QStringLiteral("arrow") },
        { QStringLiteral("start"), pt(500, 240) },
        { QStringLiteral("end"), pt(400, 300) },
        { QStringLiteral("color"), QStringLiteral("#f59e0b") },
        { QStringLiteral("width"), 4 } });
    session.addOperation(QJsonObject{
        { QStringLiteral("type"), QStringLiteral("text") },
        { QStringLiteral("point"), pt(220, 480) },
        { QStringLiteral("text"), QStringLiteral("示例标注") },
        { QStringLiteral("color"), QStringLiteral("#ffffff") },
        { QStringLiteral("size"), 28 } });
    session.addOperation(QJsonObject{
        { QStringLiteral("type"), QStringLiteral("number") },
        { QStringLiteral("point"), pt(850, 250) },
        { QStringLiteral("text"), QStringLiteral("1") },
        { QStringLiteral("color"), QStringLiteral("#e11d48") },
        { QStringLiteral("size"), 26 } });

    qmlRegisterType<sc::FrameImage>("Frap", 1, 0, "FrameImage");
    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.engine()->addImportPath(QStringLiteral("qrc:/"));
    view.engine()->addImageProvider(QStringLiteral("icons"), new IconProvider);
    view.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    view.setSource(QUrl(QStringLiteral("qrc:/Frap/main.qml")));
    view.resize(1600, 900);
    view.show();

    QTimer::singleShot(1200, [&]() {
        backend.setActiveTool(QStringLiteral("tool.rect"));
        QTimer::singleShot(400, [&]() {
            QImage grab = view.grabWindow();
            grab.save(outPath);
            qInfo() << "已输出:" << outPath << grab.size();
            app.exit(0);
        });
    });

    return app.exec();
}
