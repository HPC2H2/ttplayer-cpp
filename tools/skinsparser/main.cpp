/*
 * TTPlayer SkinsParser - 独立命令行工具
 *
 * 用法:
 *   ttplayer-skinsparser list    <file.skn>              列出 .skn 内文件
 *   ttplayer-skinsparser extract <file.skn> [output_dir]  解压 .skn 到目录
 *   ttplayer-skinsparser info    <file.skn | dir>         显示皮肤信息
 *   ttplayer-skinsparser map     <file.skn | dir>         显示 UI 元素映射表
 */
#include "skinparser.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTextStream>

static void printUsage(const QString& appName)
{
    QTextStream out(stdout);
    out << "============================================\n";
    out << "  TTPlayer Skin Parser (skinsparser) v1.0\n";
    out << "  千千静听 .skn 皮肤文件解析工具\n";
    out << "============================================\n\n";
    out << "用法:\n";
    out << "  " << appName << " list    <file.skn>              - 列出 .skn 内的文件清单\n";
    out << "  " << appName << " extract <file.skn> [output_dir] - 解压 .skn 到指定目录\n";
    out << "  " << appName << " info    <file.skn | dir>        - 显示皮肤基本信息\n";
    out << "  " << appName << " map     <file.skn | dir>        - 显示完整的 UI 元素映射表\n\n";
    out << "说明: .skn 文件实际上是 ZIP 压缩包格式，内含 BMP 图片和 XML 配置。\n";
}

static void printSeparator()
{
    qDebug() << "------------------------------------------------------------------------";
}

static void printElement(const SkinElement& elem, int indent = 2)
{
    QString prefix(indent, ' ');
    qDebug().nospace() << prefix << "[" << elem.name << "]";
    if (!elem.position.isEmpty())
        qDebug().nospace() << prefix << "  position=" << elem.position.x() << "," << elem.position.y()
                           << "+" << elem.position.width() << "x" << elem.position.height();
    if (!elem.image.isEmpty())
        qDebug().nospace() << prefix << "  image=" << elem.image;
    if (!elem.thumbImage.isEmpty())
        qDebug().nospace() << prefix << "  thumb_image=" << elem.thumbImage;
    if (!elem.fillImage.isEmpty())
        qDebug().nospace() << prefix << "  fill_image=" << elem.fillImage;
    if (!elem.barImage.isEmpty())
        qDebug().nospace() << prefix << "  bar_image=" << elem.barImage;
    if (elem.color.isValid())
        qDebug().nospace() << prefix << "  color=" << elem.color.name();
    if (!elem.font.isEmpty()) {
        qDebug().nospace() << prefix << "  font=" << elem.font << " " << elem.fontSize << "px";
    }
}

static void printWindowInfo(const SkinWindow& win)
{
    qDebug() << "\n=== " << win.name << " ===";
    if (!win.image.isEmpty())
        qDebug() << "  背景图片:" << win.image;
    if (!win.position.isEmpty())
        qDebug() << "  位置/尺寸:" << win.position.x() << "," << win.position.y()
                  << "+" << win.position.width() << "x" << win.position.height();

    if (!win.elements.isEmpty()) {
        qDebug() << "  UI 元素 (" << win.elements.size() << "个):";
        for (const auto& elem : win.elements) {
            printElement(elem);
        }
    }
}

// ========== 命令处理 ==========

int cmdList(const QStringList& args)
{
    if (args.size() < 1) {
        qWarning() << "错误: 缺少 .skn 文件路径";
        return 1;
    }

    QString sknPath = args[0];
    if (!SkinParser::isValidSknFile(sknPath)) {
        qWarning() << "错误: 无效的 .skn 文件:" << sknPath;
        return 1;
    }

    QStringList contents = SkinParser::listSknContents(sknPath);

    printSeparator();
    qDebug() << "文件:" << QFileInfo(sknPath).fileName();
    qDebug() << "内容 (" << contents.size() << " 个文件):";
    printSeparator();

    for (const QString& f : contents) {
        qDebug() << "  " << f;
    }

    // 分类统计
    int bmpCount = 0, xmlCount = 0, otherCount = 0;
    for (const QString& f : contents) {
        if (f.endsWith(".bmp", Qt::CaseInsensitive)) bmpCount++;
        else if (f.endsWith(".xml", Qt::CaseInsensitive)) xmlCount++;
        else otherCount++;
    }
    printSeparator();
    qDebug() << "统计: BMP(" << bmpCount << ") XML(" << xmlCount << ") 其他(" << otherCount << ")";
    return 0;
}

int cmdExtract(const QStringList& args)
{
    if (args.size() < 1) {
        qWarning() << "错误: 缺少 .skn 文件路径";
        return 1;
    }

    QString sknPath = args[0];
    QString destDir = (args.size() >= 2) ? args[1]
                     : QFileInfo(sknPath).completeBaseName(); // 默认用文件名（去掉扩展名）作为输出目录

    if (!SkinParser::isValidSknFile(sknPath)) {
        qWarning() << "错误: 无效的 .skn 文件:" << sknPath;
        return 1;
    }

    qDebug() << "解压中...";
    qDebug() << "  源文件:" << sknPath;
    qDebug() << "  目标目录:" << destDir;

    if (SkinParser::extractSknFile(sknPath, destDir)) {
        qDebug() << "\n解压成功! 共 "
                 << QDir(destDir).entryList(QDir::Files | QDir::NoDotAndDotDot).size()
                 << " 个文件";
        return 0;
    } else {
        qWarning() << "解压失败!";
        return 1;
    }
}

int cmdInfo(const QStringList& args)
{
    if (args.size() < 1) {
        qWarning() << "错误: 缺少路径参数";
        return 1;
    }

    QString path = args[0];
    SkinConfig config;
    QString tempDir;

    bool ok;
    if (path.toLower().endsWith(".skn")) {
        ok = SkinParser::loadFromSknFile(path, config, tempDir);
    } else {
        ok = SkinParser::loadFromDirectory(path, config);
    }

    if (!ok) {
        qWarning() << "无法加载皮肤配置";
        return 1;
    }

    printSeparator();
    qDebug() << "皮肤名称:" << config.name;
    qDebug() << "版本:   v" << config.version;
    qDebug() << "作者:   " << (config.author.isEmpty() ? "(未知)" : config.author);
    if (!config.url.isEmpty())
        qDebug() << "网站:   " << config.url;
    if (!config.email.isEmpty())
        qDebug() << "邮箱:   " << config.email;
    if (config.transparentColor.isValid())
        qDebug() << "透明色: " << config.transparentColor.name();

    // 各窗口摘要
    qDebug() << "\n窗口:";
    qDebug() << "  主播放器窗口 (player_window):"
             << config.playerWindow.elements.size() << "个元素,"
             << (config.playerWindow.image.isEmpty() ? "无背景图" : config.playerWindow.image);
    qDebug() << "  歌词窗口 (lyric_window):"
             << config.lyricWindow.elements.size() << "个元素,"
             << (config.lyricWindow.image.isEmpty() ? "无背景图" : config.lyricWindow.image);
    qDebug() << "  均衡器窗口 (equalizer_window):"
             << config.equalizerWindow.elements.size() << "个元素,"
             << (config.equalizerWindow.image.isEmpty() ? "无背景图" : config.equalizerWindow.image);
    qDebug() << "  播放列表窗口 (playlist_window):"
             << config.playlistWindow.elements.size() << "个元素,"
             << (config.playlistWindow.image.isEmpty() ? "无背景图" : config.playlistWindow.image);

    // 颜色方案摘要
    qDebug() << "\n颜色方案:";
    qDebug() << "  频谱: top=" << config.visual.spectrumTop.name()
             << " mid=" << config.visual.spectrumMid.name()
             << " bottom=" << config.visual.spectrumBottom.name()
             << " peak=" << config.visual.spectrumPeak.name();
    qDebug() << "  歌词: text=" << config.lyric.textColor.name()
             << " highlight=" << config.lyric.highlightColor.name()
             << " font=" << config.lyric.fontFamily;
    qDebug() << "  播放列表: text=" << config.playlist.textColor.name()
             << " highlight=" << config.playlist.highlightColor.name();

    printSeparator();

    // 清理临时文件
    if (!tempDir.isEmpty())
        SkinParser::cleanupTempDir(tempDir);

    return 0;
}

int cmdMap(const QStringList& args)
{
    if (args.size() < 1) {
        qWarning() << "错误: 缺少路径参数";
        return 1;
    }

    QString path = args[0];
    SkinConfig config;
    QString tempDir;

    bool ok;
    if (path.toLower().endsWith(".skn")) {
        ok = SkinParser::loadFromSknFile(path, config, tempDir);
    } else {
        ok = SkinParser::loadFromDirectory(path, config);
    }

    if (!ok) {
        qWarning() << "无法加载皮肤配置";
        return 1;
    }

    // 打印完整的 UI 元素映射表
    printSeparator();
    qDebug() << "皮肤: " << config.name << " (" << config.author << ")";
    qDebug() << "UI 元素完整映射表";
    printSeparator();

    // 主播放器窗口
    printWindowInfo(config.playerWindow);

    // 歌词窗口
    if (!config.lyricWindow.elements.isEmpty() || !config.lyricWindow.image.isEmpty()) {
        printWindowInfo(config.lyricWindow);
    }

    // 均衡器窗口
    if (!config.equalizerWindow.elements.isEmpty() || !config.equalizerWindow.image.isEmpty()) {
        printWindowInfo(config.equalizerWindow);
    }

    // 播放列表窗口
    if (!config.playlistWindow.elements.isEmpty() || !config.playlistWindow.image.isEmpty()) {
        printWindowInfo(config.playlistWindow);
    }

    // 打印人类可读的映射摘要
    printSeparator();
    qDebug() << "\n=== 映射摘要 (ttplayer UI -> 图片文件) ===\n";

    struct MappingEntry {
        const char* uiDesc;
        const char* elemName;
    };

    // 主播放器常用元素
    MappingEntry playerMappings[] = {
        {"播放按钮",          "play"},
        {"暂停按钮",          "pause"},
        {"停止按钮",          "stop"},
        {"上一首按钮",        "prev"},
        {"下一首按钮",        "next"},
        {"静音按钮",          "mute"},
        {"打开文件按钮",      "open"},
        {"歌词按钮",          "lyric"},
        {"均衡器按钮",        "equalizer"},
        {"播放列表按钮",      "playlist"},
        {"最小化按钮",        "minimize"},
        {"关闭/退出按钮",     "exit"},
        {"进度条",            "progress"},
        {"音量滑块",          "volume"},
        {"频谱显示区域",      "visual"},
        {"图标区域",          "icon"},
        {"歌曲信息文本区",    "info"},
        {"LED 数字显示",      "led"},
        {"立体声指示",        "stereo"},
        {"状态信息",          "status"},
        {nullptr, nullptr}
    };

    for (int i = 0; playerMappings[i].uiDesc; ++i) {
        const SkinElement* elem = config.findElement("player_window", playerMappings[i].elemName);
        if (elem) {
            QString imageText = elem->image;
            if (elem->thumbImage.isEmpty() && imageText.isEmpty())
                imageText = "(纯绘制区域,无图片)";
            else if (!elem->thumbImage.isEmpty())
                imageText += " [thumb:" + elem->thumbImage + "]";

            qDebug() << "  " << playerMappings[i].uiDesc << " (" << playerMappings[i].elemName << ")"
                     << "->" << imageText;
            if (!elem->position.isEmpty()) {
                qDebug() << "       位置: x=" << elem->position.x()
                         << " y=" << elem->position.y()
                         << " size=" << elem->position.width()
                         << "x" << elem->position.height();
            }
        } else {
            qDebug() << "  " << playerMappings[i].uiDesc << " (" << playerMappings[i].elemName << ")"
                     << "-> (未定义)";
        }
    }

    printSeparator();

    // 清理临时文件
    if (!tempDir.isEmpty())
        SkinParser::cleanupTempDir(tempDir);

    return 0;
}

// ========== 入口 ==========

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();

    if (args.size() < 2) {
        printUsage(args[0]);
        return 1;
    }

    QString command = args[1].toLower();
    QStringList commandArgs = args.mid(2);

    if (command == "list")
        return cmdList(commandArgs);
    else if (command == "extract")
        return cmdExtract(commandArgs);
    else if (command == "info")
        return cmdInfo(commandArgs);
    else if (command == "map")
        return cmdMap(commandArgs);
    else if (command == "--help" || command == "-h" || command == "/?") {
        printUsage(args[0]);
        return 0;
    }
    else {
        qWarning() << "未知命令:" << command << "\n";
        printUsage(args[0]);
        return 1;
    }
}
