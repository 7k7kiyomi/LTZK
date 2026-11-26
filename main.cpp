#include "LTZK.h"
#include "LTZKUiScale.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QPixmap>
#include <QScreen>
#include <QSplashScreen>
#include <QStringList>
#include <QToolTip>
#include <QTranslator>
#include <QVariant>
#include <cstdio>
#include "qgsapplication.h"
#include "qgseditorwidgetregistry.h"
#include "qgsgui.h"
namespace
{
QString appDirFromArgv0(int argc, char* argv[])
{
    if (argc > 0 && argv && argv[0] && argv[0][0] != '\0')
    {
        const QFileInfo exeInfo(QString::fromLocal8Bit(argv[0]));
        const QString absoluteDir = exeInfo.absoluteDir().absolutePath();
        if (!absoluteDir.isEmpty() && QDir(absoluteDir).exists())
        {
            return absoluteDir;
        }
    }

    return QDir::currentPath();
}

QString findQgisPrefixPath(const QString& appDir)
{
    const QString envPath = QDir::fromNativeSeparators(qEnvironmentVariable("QGIS_PREFIX_PATH"));
    if (!envPath.isEmpty() && QDir(envPath).exists())
    {
        return QDir(envPath).absolutePath();
    }

    const QStringList candidates = {
        appDir + QStringLiteral("/QGIS_32815"),
        appDir + QStringLiteral("/../QGIS_32815"),
        QStringLiteral("E:/extern/QGIS_32815")
    };

    for (const QString& path : candidates)
    {
        const QDir dir(path);
        if (dir.exists())
        {
            return dir.absolutePath();
        }
    }

    return QString();
}

void setRuntimePath(const char* name, const QString& path)
{
    if (!path.isEmpty())
    {
        qputenv(name, QDir::toNativeSeparators(QDir::cleanPath(path)).toLocal8Bit());
    }
}

QString findOsgeoRoot(const QString& qgisPrefixPath, const QString& appDir)
{
    const QString qgisParentDir = QFileInfo(qgisPrefixPath).absolutePath();
    const QStringList candidates = {
        appDir + QStringLiteral("/OSGeo4W_32815"),
        appDir + QStringLiteral("/../OSGeo4W_32815"),
        qgisParentDir + QStringLiteral("/OSGeo4W_32815"),
        QStringLiteral("E:/extern/OSGeo4W_32815")
    };

    for (const QString& path : candidates)
    {
        const QDir dir(path);
        if (dir.exists(QStringLiteral("share/proj/proj.db")))
        {
            return dir.absolutePath();
        }
    }

    return QString();
}

void configureQgisRuntimeEnvironment(const QString& qgisPrefixPath, const QString& appDir)
{
    setRuntimePath("QGIS_PREFIX_PATH", qgisPrefixPath);

    const QString osgeoRoot = findOsgeoRoot(qgisPrefixPath, appDir);
    if (osgeoRoot.isEmpty())
    {
        return;
    }

    const QString projDataPath = osgeoRoot + QStringLiteral("/share/proj");
    const QString gdalDataPath = osgeoRoot + QStringLiteral("/share/gdal");
    const QString qtPluginPath = osgeoRoot + QStringLiteral("/apps/Qt5/plugins");
    const QString qtPlatformPluginPath = qtPluginPath + QStringLiteral("/platforms");

    if (QFileInfo(projDataPath + QStringLiteral("/proj.db")).exists())
    {
        setRuntimePath("PROJ_LIB", projDataPath);
        setRuntimePath("PROJ_DATA", projDataPath);
    }
    if (QDir(gdalDataPath).exists())
    {
        setRuntimePath("GDAL_DATA", gdalDataPath);
    }
    if (QDir(qtPluginPath).exists())
    {
        setRuntimePath("QT_PLUGIN_PATH", qtPluginPath);
    }
    if (QDir(qtPlatformPluginPath).exists())
    {
        setRuntimePath("QT_QPA_PLATFORM_PLUGIN_PATH", qtPlatformPluginPath);
    }

    const QString runtimePath =
        QDir::toNativeSeparators(
            osgeoRoot + QStringLiteral("/apps/Qt5/bin"))
        + QLatin1Char(';')
        + QDir::toNativeSeparators(
            osgeoRoot + QStringLiteral("/bin"))
        + QLatin1Char(';')
        + QDir::toNativeSeparators(
            qgisPrefixPath + QStringLiteral("/bin"));

    qputenv(
        "PATH",
        runtimePath.toLocal8Bit());
}
QIcon loadApplicationIcon()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/resources/LTZK.svg"),
        appDir + QStringLiteral("/Resources/LTZK.svg"),
        appDir + QStringLiteral("/../resources/LTZK.svg"),
        appDir + QStringLiteral("/../Resources/LTZK.svg")
    };

    for (const QString& path : candidates)
    {
        if (!QFileInfo(path).exists())
        {
            continue;
        }

        const QIcon runtimeIcon(path);
        if (!runtimeIcon.isNull())
        {
            return runtimeIcon;
        }
    }

    return QIcon(QStringLiteral(":/LTZK/Resources/LTZK.svg"));
}
}

int main(int argc, char *argv[])
{
    const QString appDir = appDirFromArgv0(argc, argv);
    const QString qgisPrefixPath = findQgisPrefixPath(appDir);
    if (qgisPrefixPath.isEmpty())
    {
        std::fprintf(stderr, "QGIS runtime path was not found. Please start LTZK with start_LTZK.bat or set QGIS_PREFIX_PATH.\n");
        return -1;
    }

    configureQgisRuntimeEnvironment(qgisPrefixPath, appDir);

    /*
     * 先让 Qt 按 Windows DPI 生成逻辑坐标，再由 LTZK 按逻辑分辨率
     * 计算 1920x1080 视觉比例，避免物理像素和系统缩放被重复计算。
     */
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    QgsApplication a(argc, argv, true);
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
    QApplication::setEffectEnabled(Qt::UI_FadeTooltip, false);

    const QFont baseApplicationFont = a.font();
    qApp->setProperty("ltzkBaseApplicationFont", QVariant::fromValue(baseApplicationFont));
    a.setFont(LTZKUiScale::scaledFont(
        baseApplicationFont,
        LTZKUiScale::factorForScreen(QGuiApplication::primaryScreen())));
    QToolTip::setFont(a.font());

    QTranslator qtBaseChineseTranslator;
    QTranslator qtChineseTranslator;
    QTranslator qgisChineseTranslator;

    a.setWindowIcon(loadApplicationIcon());  // 设置应用程序窗口图标。

    QPixmap splashPixmap(":/LTZK/Resources/splash.png");
    QScreen* splashScreen = QGuiApplication::primaryScreen();
    if (splashScreen)
    {
        const QRect availableGeometry = splashScreen->availableGeometry();
        const QSize maxSplashSize(
            qMin(840, availableGeometry.width() * 45 / 100),
            qMin(450, availableGeometry.height() * 45 / 100));
        splashPixmap = splashPixmap.scaled(
            maxSplashSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
    }

    QSplashScreen splash(splashPixmap);
    if (splashScreen)
    {
        const QRect availableGeometry = splashScreen->availableGeometry();
        splash.move(availableGeometry.center() - splash.rect().center());
    }
    splash.show();
    a.processEvents();
    QgsApplication::setPrefixPath(qgisPrefixPath, true);
    // Keep embedded QGIS widgets (including the attribute table and feature forms) in Simplified Chinese.
    const QStringList qgisTranslationPaths = {
        QgsApplication::i18nPath(),
        QDir(qgisPrefixPath).filePath(QStringLiteral("i18n")),
        QDir(qgisPrefixPath).filePath(QStringLiteral("output/i18n")),
        QDir(appDir).filePath(QStringLiteral("i18n"))
    };
    for (const QString& translationPath : qgisTranslationPaths)
    {
        if (qgisChineseTranslator.load(QStringLiteral("qgis_zh-Hans"), translationPath))
        {
            break;
        }
    }

    const QString osgeoRoot = findOsgeoRoot(qgisPrefixPath, appDir);
    const QStringList qtTranslationPaths = {
        QDir(osgeoRoot).filePath(QStringLiteral("apps/Qt5/translations")),
        QDir(qgisPrefixPath).filePath(QStringLiteral("apps/Qt5/translations")),
        QDir(appDir).filePath(QStringLiteral("translations"))
    };
    for (const QString& translationPath : qtTranslationPaths)
    {
        if (qtBaseChineseTranslator.load(QStringLiteral("qtbase_zh_CN"), translationPath))
        {
            break;
        }
    }
    for (const QString& translationPath : qtTranslationPaths)
    {
        if (qtChineseTranslator.load(QStringLiteral("qt_zh_CN"), translationPath))
        {
            break;
        }
    }

    QgsApplication::setTranslation(QStringLiteral("zh-Hans"));

    QgsApplication::init();
    QgsApplication::initQgis();

    // Install translations after QGIS startup so they take precedence for native dialogs.
    if (!qgisChineseTranslator.isEmpty())
    {
        a.installTranslator(&qgisChineseTranslator);
    }
    if (!qtBaseChineseTranslator.isEmpty())
    {
        a.installTranslator(&qtBaseChineseTranslator);
    }
    if (!qtChineseTranslator.isEmpty())
    {
        a.installTranslator(&qtChineseTranslator);
    }

    /*
     * 在创建 QgsMapCanvas、QgsLayerTreeView 等
     * QGIS GUI 控件前显式初始化 GUI 单例。
     */
    QgsGui::instance();
    // Standalone QGIS applications must register the built-in field editors
    // before attribute forms are created (for example, while adding features).
    QgsGui::editorWidgetRegistry()->initEditors();

    int exitCode = 0;

    {
        LTZK w;
        w.showMaximized();
        splash.finish(&w);

        exitCode = a.exec();
    }

    /*
     * 此时 LTZK 主窗口已经析构，
     * 再释放 QGIS 全局资源。
     */
    QgsApplication::exitQgis();

    return exitCode;
}

