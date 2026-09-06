/*
 * qml_controller_test —— 驱动真实 QML CaptureController 的交互测试
 *
 * 通过 QMetaObject 调用 QML 状态机的 press/move/release/onKey，
 * 断言 backend/会话状态。覆盖：
 *   工具条模型、全屏绘制、裁剪、标注选中/移动/删除、撤销/重做、
 *   快捷键、文字/编号、完成导出干净退出。
 */
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QDir>
#include <QImage>
#include <QEventLoop>
#include <QTimer>
#include <QTest>
#include <QSignalSpy>
#include <QDebug>

#include "settings.h"
#include "plugin_manager.h"
#include "session.h"
#include "qml_backend.h"
#include "frame_image.h"
#include "icon_provider.h"
#include "logging.h"

using namespace sc;

static void invoke(QObject* o, const char* fn,
                   QVariant a = QVariant(), QVariant b = QVariant(), QVariant c = QVariant())
{
    bool ok = false;
    if (a.isValid() && b.isValid() && c.isValid())
        ok = QMetaObject::invokeMethod(o, fn, Q_ARG(QVariant, a), Q_ARG(QVariant, b),
                                       Q_ARG(QVariant, c));
    else if (a.isValid() && b.isValid())
        ok = QMetaObject::invokeMethod(o, fn, Q_ARG(QVariant, a), Q_ARG(QVariant, b));
    else if (a.isValid())
        ok = QMetaObject::invokeMethod(o, fn, Q_ARG(QVariant, a));
    else
        ok = QMetaObject::invokeMethod(o, fn);
    if (!ok)
        qWarning() << "invokeMethod failed:" << fn;
}

static bool invokeBool(QObject* o, const char* fn,
                       QVariant a = QVariant(), QVariant b = QVariant(), QVariant c = QVariant())
{
    bool out = false;
    if (a.isValid() && b.isValid() && c.isValid())
        QMetaObject::invokeMethod(o, fn, Q_RETURN_ARG(bool, out), Q_ARG(QVariant, a),
                                  Q_ARG(QVariant, b), Q_ARG(QVariant, c));
    else if (a.isValid() && b.isValid())
        QMetaObject::invokeMethod(o, fn, Q_RETURN_ARG(bool, out), Q_ARG(QVariant, a),
                                  Q_ARG(QVariant, b));
    else if (a.isValid())
        QMetaObject::invokeMethod(o, fn, Q_RETURN_ARG(bool, out), Q_ARG(QVariant, a));
    else
        QMetaObject::invokeMethod(o, fn, Q_RETURN_ARG(bool, out));
    return out;
}

class QmlControllerTest : public QObject
{
    Q_OBJECT
public:
    QmlControllerTest(const QString& pluginDir)
        : m_pluginDir(pluginDir)
    {}

private slots:
    void initTestCase()
    {
        installConsoleLogging();
        const QString configRoot = QDir::tempPath() + QStringLiteral("/myscr_ctrl_config");
        QDir().mkpath(configRoot);
        qputenv("XDG_CONFIG_HOME", configRoot.toUtf8());
        m_exportDir = QDir::tempPath() + QStringLiteral("/myscr_ctrl_export");
        QDir().mkpath(m_exportDir);

        m_settings = new Settings;
        m_settings->setPluginDirectories({ m_pluginDir });
        m_settings->setValue(QStringLiteral("export.save.directory"), m_exportDir);

        m_session = new CaptureSession;
        QImage image(1600, 900, QImage::Format_ARGB32);
        image.fill(QColor(120, 140, 160));
        m_session->setImage(image);
        m_session->setScreenSize(QSizeF(1600, 900));

        m_manager = new PluginManager(m_settings);
        m_manager->start();
        QEventLoop loop;
        connect(m_manager, &PluginManager::ready, &loop, &QEventLoop::quit);
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        loop.exec();
        QVERIFY(m_manager->isReady());

        m_backend = new FrapBackend(m_session, m_settings, m_manager);

        m_engine = new QQmlEngine;
        m_engine->addImportPath(QStringLiteral("qrc:/"));
        m_engine->addImageProvider(QStringLiteral("icons"), new IconProvider);
        m_engine->rootContext()->setContextProperty(QStringLiteral("backend"), m_backend);
        qmlRegisterType<sc::FrameImage>("Frap", 1, 0, "FrameImage");

        /* 加载真实 QML 状态机 */
        QQmlComponent comp(m_engine,
                           QUrl(QStringLiteral("qrc:/Frap/CaptureController.qml")));
        QVERIFY2(!comp.isError(), qPrintable(comp.errorString()));
        m_controller = comp.create();
        QVERIFY(m_controller);

        /* 加载主 UI 验证可实例化 */
        QQmlComponent ui(m_engine,
                         QUrl(QStringLiteral("qrc:/Frap/main.qml")));
        if (ui.isError())
            for (const auto& e : ui.errors())
                qWarning() << "UIERR" << e.toString();
        QVERIFY2(!ui.isError(), qPrintable(ui.errorString()));
        m_ui = ui.create();
        QVERIFY(m_ui);
    }

    void cleanupTestCase()
    {
        delete m_ui;
        delete m_controller;
        delete m_engine;
        delete m_backend;
        delete m_manager;
        delete m_session;
        delete m_settings;
    }

    void test_toolbar()
    {
        /* 等待工具插件真正进入工具条模型（非仅原生按钮） */
        QTRY_VERIFY_WITH_TIMEOUT([&]() {
            const QVariantList tb = m_backend->toolbar();
            for (const QVariant& v : tb)
                if (v.toMap().value("type").toString() == "tool"
                    && v.toMap().value("id").toString() == "tool.rect")
                    return true;
            return false;
        }(), 8000);

        QStringList ids;
        const QVariantList tb = m_backend->toolbar();
        for (const QVariant& v : tb)
            ids << v.toMap().value("id").toString();
        QVERIFY(ids.contains("tool.crop"));
        QVERIFY(ids.contains("tool.rect"));
        QVERIFY(ids.contains("tool.arrow"));
        QVERIFY(ids.contains("export.clipboard"));
        QVERIFY(ids.contains("export.save"));
        QVERIFY(ids.contains("done"));
        QVERIFY(ids.contains("service.translate"));
        QVERIFY(ids.contains("capture.longshot"));
        QCOMPARE(ids.count("export.clipboard"), 1);
    }

    void test_draw_rect()
    {
        m_backend->setActiveTool("tool.rect");
        QCOMPARE(m_backend->activeTool(), QStringLiteral("tool.rect"));
        invoke(m_controller, "press", 300.0, 300.0, int(Qt::LeftButton));
        invoke(m_controller, "move", 600.0, 500.0);
        invoke(m_controller, "release");
        QTRY_COMPARE_WITH_TIMEOUT(m_backend->opCount(), 1, 5000);
        QCOMPARE(m_controller->property("mode").toInt(), 0); /* 空闲 */
    }

    void test_crop()
    {
        m_backend->toggleCropTool();
        QVERIFY(m_backend->cropToolActive());
        invoke(m_controller, "press", 100.0, 100.0, int(Qt::LeftButton));
        invoke(m_controller, "move", 700.0, 500.0);
        invoke(m_controller, "release");
        QVERIFY(m_backend->hasCrop());
        QRect c = m_backend->cropRect();
        QCOMPARE(c.x(), 100);
        QCOMPARE(c.y(), 100);
        QVERIFY(c.width() >= 599 && c.width() <= 601);
        /* 移动裁剪框 */
        invoke(m_controller, "press", 400.0, 300.0, int(Qt::LeftButton));
        invoke(m_controller, "move", 450.0, 330.0);
        invoke(m_controller, "release");
        QCOMPARE(m_backend->cropRect().x(), 150);
        QCOMPARE(m_backend->cropRect().y(), 130);
        m_backend->deactivateTool();
    }

    void test_select_move_delete()
    {
        /* 点击矩形中部选中并移动 */
        invoke(m_controller, "press", 450.0, 400.0, int(Qt::LeftButton));
        QVERIFY(m_controller->property("selectedOp").toString().length() > 0);
        invoke(m_controller, "move", 500.0, 430.0);
        invoke(m_controller, "release");
        /* Delete 删除 */
        invoke(m_controller, "onKey", int(Qt::Key_Delete), QString(), int(Qt::NoModifier));
        QTRY_COMPARE_WITH_TIMEOUT(m_backend->opCount(), 0, 3000);
    }

    void test_undo_redo()
    {
        m_backend->setActiveTool("tool.rect");
        invoke(m_controller, "press", 200.0, 200.0, int(Qt::LeftButton));
        invoke(m_controller, "move", 500.0, 400.0);
        invoke(m_controller, "release");
        QTRY_COMPARE_WITH_TIMEOUT(m_backend->opCount(), 1, 5000);
        invoke(m_controller, "onKey", int(Qt::Key_Z), QString(), int(Qt::ControlModifier));
        QCOMPARE(m_backend->opCount(), 0);
        invoke(m_controller, "onKey", int(Qt::Key_Z), QString(),
                        int(Qt::ControlModifier | Qt::ShiftModifier));
        QCOMPARE(m_backend->opCount(), 1);
        m_backend->deactivateTool();
    }

    void test_shortcuts()
    {
        invoke(m_controller, "onKey", int(Qt::Key_R), QStringLiteral("r"), int(Qt::NoModifier));
        QCOMPARE(m_backend->activeTool(), QStringLiteral("tool.rect"));
        invoke(m_controller, "onKey", int(Qt::Key_Escape), QString(), int(Qt::NoModifier));
        QCOMPARE(m_backend->activeTool(), QString());
    }

    void test_text_tool()
    {
        m_backend->setActiveTool("tool.text");
        invoke(m_controller, "press", 300.0, 300.0, int(Qt::LeftButton));
        QVERIFY(m_controller->property("textFocused").toBool());
        invoke(m_controller, "onKey", int(Qt::Key_H), QStringLiteral("h"), int(Qt::NoModifier));
        invoke(m_controller, "onKey", int(Qt::Key_I), QStringLiteral("i"), int(Qt::NoModifier));
        QCOMPARE(m_controller->property("textBuffer").toString(), QStringLiteral("hi"));
        invoke(m_controller, "onKey", int(Qt::Key_Return), QString(), int(Qt::NoModifier));
        QTRY_VERIFY_WITH_TIMEOUT(m_backend->opCount() >= 2, 5000);
        m_backend->deactivateTool();
    }

    void test_number_tool()
    {
        m_backend->setActiveTool("tool.number");
        invoke(m_controller, "press", 250.0, 250.0, int(Qt::LeftButton));
        QTRY_VERIFY_WITH_TIMEOUT(m_backend->opCount() >= 3, 5000);
        m_backend->deactivateTool();
    }

    void test_right_click_cancel()
    {
        m_backend->setActiveTool("tool.rect");
        QVERIFY(!m_backend->activeTool().isEmpty());
        invoke(m_controller, "press", 800.0, 600.0, int(Qt::RightButton));
        QCOMPARE(m_backend->activeTool(), QString());
    }

    void test_finish()
    {
        QSignalSpy spy(m_backend, &FrapBackend::sessionFinished);
        m_backend->finish();
        QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 6000);
    }

private:
    QString m_pluginDir;
    QString m_exportDir;
    Settings* m_settings = nullptr;
    CaptureSession* m_session = nullptr;
    PluginManager* m_manager = nullptr;
    FrapBackend* m_backend = nullptr;
    QQmlEngine* m_engine = nullptr;
    QObject* m_controller = nullptr;
    QObject* m_ui = nullptr;
};

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    const QString pluginDir = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                       : QStringLiteral("plugins");
    QmlControllerTest tc(pluginDir);
    char prog[] = "qml_controller_test";
    char* qargv[] = { prog, nullptr };
    return QTest::qExec(&tc, 1, qargv);
}

#include "qml_controller_test.moc"
