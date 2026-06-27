/*
 * TTPlayer Skin Parser 实现
 */
#include "skinparser.h"
#include <QFile>
#include <QDir>
#include <QXmlStreamReader>
#include <QTemporaryDir>
#include <QuaZip-Qt7-1.5/quazip.h>
#include <QuaZip-Qt7-1.5/quazipfile.h>
#include <QDebug>

// ========== SkinConfig 辅助方法 ==========

const SkinElement* SkinConfig::findElement(const QString& windowName, const QString& elemName) const
{
    const SkinWindow* win = getWindow(windowName);
    if (!win) return nullptr;

    for (const auto& elem : win->elements) {
        if (elem.name == elemName)
            return &elem;
    }
    return nullptr;
}

const SkinWindow* SkinConfig::getWindow(const QString& windowName) const
{
    if (windowName == "player_window" || windowName == "player") return &playerWindow;
    if (windowName == "lyric_window" || windowName == "lyric") return &lyricWindow;
    if (windowName == "equalizer_window" || windowName == "equalizer") return &equalizerWindow;
    if (windowName == "playlist_window" || windowName == "playlist") return &playlistWindow;
    return nullptr;
}

// ========== 解析辅助函数 ==========

QRect SkinParser::parseRect(const QString& rectStr)
{
    // 格式: "left, top, right, bottom"
    QStringList parts = rectStr.split(',', Qt::SkipEmptyParts);
    if (parts.size() >= 4) {
        int l = parts[0].trimmed().toInt();
        int t = parts[1].trimmed().toInt();
        int r = parts[2].trimmed().toInt();
        int b = parts[3].trimmed().toInt();
        return QRect(l, t, r - l, b - t);
    }
    return QRect();
}

QColor SkinParser::parseColor(const QString& colorStr)
{
    QString c = colorStr.trimmed();
    if (c.startsWith('#')) {
        return QColor(c);
    }
    return QColor(c);
}

// ========== XML 解析 ==========

bool SkinParser::parseSkinXml(const QString& xmlPath, SkinConfig& config)
{
    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开 Skin.xml:" << xmlPath;
        return false;
    }

    QXmlStreamReader xml(&file);
    SkinWindow *currentWindow = nullptr;

    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();

        if (xml.isStartElement()) {
            QString name = xml.name().toString();

            if (name == "skin") {
                config.version = xml.attributes().value("version").toString().toInt();
                config.name = xml.attributes().value("name").toString();
                config.author = xml.attributes().value("author").toString();
                config.url = xml.attributes().value("url").toString();
                config.email = xml.attributes().value("email").toString();
                config.transparentColor = parseColor(xml.attributes().value("transparent_color").toString());
            }
            else if (name == "player_window") {
                currentWindow = &config.playerWindow;
                currentWindow->name = "player_window";
                currentWindow->image = xml.attributes().value("image").toString();
                currentWindow->transparentColor = config.transparentColor;
            }
            else if (name == "lyric_window") {
                currentWindow = &config.lyricWindow;
                currentWindow->name = "lyric_window";
                currentWindow->image = xml.attributes().value("image").toString();
                currentWindow->position = parseRect(xml.attributes().value("position").toString());
                QString rr = xml.attributes().value("resize_rect").toString();
                if (!rr.isEmpty()) currentWindow->resizeRect = parseRect(rr);
                currentWindow->resizeTile = xml.attributes().value("resize_tile").toString().toInt();
            }
            else if (name == "equalizer_window") {
                currentWindow = &config.equalizerWindow;
                currentWindow->name = "equalizer_window";
                currentWindow->image = xml.attributes().value("image").toString();
                currentWindow->position = parseRect(xml.attributes().value("position").toString());
                currentWindow->eqInterval = xml.attributes().value("eq_interval").toString().toInt();
            }
            else if (name == "playlist_window") {
                currentWindow = &config.playlistWindow;
                currentWindow->name = "playlist_window";
                currentWindow->image = xml.attributes().value("image").toString();
                currentWindow->position = parseRect(xml.attributes().value("position").toString());
                QString rr = xml.attributes().value("resize_rect").toString();
                if (!rr.isEmpty()) currentWindow->resizeRect = parseRect(rr);
                currentWindow->resizeTile = xml.attributes().value("resize_tile").toString().toInt();
            }
            else if (currentWindow && name != "skin") {
                // 通用元素解析
                SkinElement elem;
                elem.name = name;

                QXmlStreamAttributes attrs = xml.attributes();

                if (attrs.hasAttribute("position")) {
                    elem.position = parseRect(attrs.value("position").toString());
                }

                if (attrs.hasAttribute("image")) elem.image = attrs.value("image").toString();
                if (attrs.hasAttribute("thumb_image")) elem.thumbImage = attrs.value("thumb_image").toString();
                if (attrs.hasAttribute("fill_image")) elem.fillImage = attrs.value("fill_image").toString();
                if (attrs.hasAttribute("bar_image")) elem.barImage = attrs.value("bar_image").toString();
                if (attrs.hasAttribute("color")) elem.color = parseColor(attrs.value("color").toString());
                if (attrs.hasAttribute("bkgnd")) elem.bkgnd = parseColor(attrs.value("bkgnd").toString());
                if (attrs.hasAttribute("font")) elem.font = attrs.value("font").toString();
                if (attrs.hasAttribute("font_size")) elem.fontSize = attrs.value("font_size").toString().toInt();
                if (attrs.hasAttribute("vertical")) elem.vertical = (attrs.value("vertical").toString() == "true");
                if (attrs.hasAttribute("align")) elem.align = attrs.value("align").toString();

                currentWindow->elements.append(elem);
            }
        }
    }

    if (xml.hasError()) {
        qWarning() << "Skin.xml 解析错误:" << xml.errorString();
        file.close();
        return false;
    }

    file.close();
    qDebug() << "成功解析皮肤配置:" << config.name
             << "- 作者:" << config.author
             << "- 窗口数: 4"
             << "- player 元素数:" << config.playerWindow.elements.size();

    return true;
}

bool SkinParser::parseVisualXml(const QString& xmlPath, SkinConfig& config)
{
    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false; // Visual.xml 可选
    }

    QXmlStreamReader xml(&file);
    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement()) {
            QString name = xml.name().toString();
            auto attrs = xml.attributes();
            if (name == "visual" || name == "spectrum") {
                if (attrs.hasAttribute("top_color"))
                    config.visual.spectrumTop = parseColor(attrs.value("top_color").toString());
                if (attrs.hasAttribute("bottom_color"))
                    config.visual.spectrumBottom = parseColor(attrs.value("bottom_color").toString());
                if (attrs.hasAttribute("mid_color"))
                    config.visual.spectrumMid = parseColor(attrs.value("mid_color").toString());
                if (attrs.hasAttribute("peak_color"))
                    config.visual.spectrumPeak = parseColor(attrs.value("peak_color").toString());
                if (attrs.hasAttribute("blur"))
                    config.visual.blurEnabled = (attrs.value("blur").toString() == "1" ||
                                                  attrs.value("blur").toString() == "true");
                if (attrs.hasAttribute("blur_speed"))
                    config.visual.blurSpeed = attrs.value("blur_speed").toString().toInt();
            }
        }
    }
    file.close();
    return true;
}

bool SkinParser::parseLyricXml(const QString& xmlPath, SkinConfig& config)
{
    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false; // 可选

    QXmlStreamReader xml(&file);
    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement()) {
            QString name = xml.name().toString();
            auto attrs = xml.attributes();
            if (name == "font" || name == "lyric") {
                if (attrs.hasAttribute("face") || attrs.hasAttribute("family"))
                    config.lyric.fontFamily = attrs.hasAttribute("face")
                        ? attrs.value("face").toString()
                        : attrs.value("family").toString();
                if (attrs.hasAttribute("size"))
                    config.lyric.fontSize = attrs.value("size").toString().toInt();
            }
            if (name == "color" || name == "text") {
                if (attrs.hasAttribute("text"))
                    config.lyric.textColor = parseColor(attrs.value("text").toString());
                if (attrs.hasAttribute("highlight"))
                    config.lyric.highlightColor = parseColor(attrs.value("highlight").toString());
            }
            if (name == "background" || name == "bkgnd") {
                if (attrs.hasAttribute("color"))
                    config.lyric.backgroundColor = parseColor(attrs.value("color").toString());
            }
        }
    }
    file.close();
    return true;
}

bool SkinParser::parsePlaylistXml(const QString& xmlPath, SkinConfig& config)
{
    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false; // 可选

    QXmlStreamReader xml(&file);
    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement()) {
            QString name = xml.name().toString();
            auto attrs = xml.attributes();

            if (name == "color" || name == "text") {
                if (attrs.hasAttribute("normal") || attrs.hasAttribute("text"))
                    config.playlist.textColor = attrs.hasAttribute("normal")
                        ? parseColor(attrs.value("normal").toString())
                        : parseColor(attrs.value("text").toString());
                if (attrs.hasAttribute("highlight") || attrs.hasAttribute("selected"))
                    config.playlist.highlightColor = attrs.hasAttribute("highlight")
                        ? parseColor(attrs.value("highlight").toString())
                        : parseColor(attrs.value("selected").toString());
                if (attrs.hasAttribute("time"))
                    config.playlist.timeColor = parseColor(attrs.value("time").toString());
            }
            if (name == "background" || name == "bkgnd") {
                if (attrs.hasAttribute("color") || attrs.hasAttribute("normal"))
                    config.playlist.backgroundColor = attrs.hasAttribute("color")
                        ? parseColor(attrs.value("color").toString())
                        : parseColor(attrs.value("normal").toString());
                if (attrs.hasAttribute("alternate"))
                    config.playlist.alternateBackground = parseColor(attrs.value("alternate").toString());
            }
            if (name == "selected" || name == "selection") {
                if (attrs.hasAttribute("color"))
                    config.playlist.selectedColor = parseColor(attrs.value("color").toString());
            }
        }
    }
    file.close();
    return true;
}

// ========== 核心 API ==========

bool SkinParser::loadFromDirectory(const QString& dirPath, SkinConfig& outConfig)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        qWarning() << "皮肤目录不存在:" << dirPath;
        return false;
    }

    // 必须有 Skin.xml
    QString skinXmlPath = dir.filePath("Skin.xml");
    if (!QFile::exists(skinXmlPath)) {
        qWarning() << "未找到 Skin.xml:" << skinXmlPath;
        return false;
    }

    // 解析主配置
    if (!parseSkinXml(skinXmlPath, outConfig))
        return false;

    // 尝试解析子配置（可选）
    QString visualXmlPath = dir.filePath("Visual.xml");
    if (QFile::exists(visualXmlPath))
        parseVisualXml(visualXmlPath, outConfig);

    QString lyricXmlPath = dir.filePath("Lyric.xml");
    if (QFile::exists(lyricXmlPath))
        parseLyricXml(lyricXmlPath, outConfig);

    QString playlistXmlPath = dir.filePath("PlayList.xml");
    if (QFile::exists(playlistXmlPath))
        parsePlaylistXml(playlistXmlPath, outConfig);

    qDebug() << "成功从目录加载皮肤:" << outConfig.name << "(" << dirPath << ")";
    return true;
}

bool SkinParser::extractSknFile(const QString& sknPath, const QString& destDir)
{
    QuaZip zip(sknPath);
    if (!zip.open(QuaZip::mdUnzip)) {
        qWarning() << "无法打开 .skn 文件（不是有效的 ZIP）:" << sknPath;
        return false;
    }

    QDir dest(destDir);
    if (!dest.exists())
        dest.mkpath(".");

    for (bool more = zip.goToFirstFile(); more; more = zip.goToNextFile()) {
        QString filePath = zip.getCurrentFileName();
        if (filePath.isEmpty()) continue;

        QuaZipFile zipFile(&zip);
        if (!zipFile.open(QIODevice::ReadOnly)) {
            qWarning() << "无法解压文件:" << filePath;
            continue;
        }

        QString fullPath = dest.filePath(filePath);
        QFileInfo fileInfo(fullPath);
        if (!fileInfo.absoluteDir().exists())
            fileInfo.absoluteDir().mkpath(".");

        QFile outFile(fullPath);
        if (outFile.open(QIODevice::WriteOnly)) {
            outFile.write(zipFile.readAll());
            outFile.close();
        }
        zipFile.close();
    }

    zip.close();
    qDebug() << "成功解压 .skn 文件到:" << destDir;
    return true;
}

bool SkinParser::loadFromSknFile(const QString& sknPath, SkinConfig& outConfig, QString& outTempDir)
{
    if (!isValidSknFile(sknPath))
        return false;

    // 创建临时目录
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        qWarning() << "无法创建临时目录";
        return false;
    }
    tempDir.setAutoRemove(false); // 手动管理生命周期
    outTempDir = tempDir.path();

    // 解压
    if (!extractSknFile(sknPath, outTempDir))
        return false;

    // 从解压后的目录加载
    return loadFromDirectory(outTempDir, outConfig);
}

QStringList SkinParser::listSknContents(const QString& sknPath)
{
    QStringList contents;
    QuaZip zip(sknPath);
    if (!zip.open(QuaZip::mdUnzip))
        return contents;

    for (bool more = zip.goToFirstFile(); more; more = zip.goToNextFile()) {
        contents.append(zip.getCurrentFileName());
    }
    zip.close();
    return contents;
}

bool SkinParser::isValidSknFile(const QString& sknPath)
{
    QFile file(sknPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    // 检查 ZIP 魔数: PK\x03\x04
    QByteArray header = file.read(4);
    file.close();

    bool valid = (header.size() >= 4 &&
                  static_cast<unsigned char>(header[0]) == 0x50 &&
                  static_cast<unsigned char>(header[1]) == 0x4B &&
                  static_cast<unsigned char>(header[2]) == 0x03 &&
                  static_cast<unsigned char>(header[3]) == 0x04);

    if (!valid)
        qWarning() << "无效的 .skn 文件（不是 ZIP 格式）:" << sknPath;

    return valid;
}

void SkinParser::cleanupTempDir(const QString& tempDir)
{
    if (tempDir.isEmpty()) return;
    QDir dir(tempDir);
    if (dir.exists()) {
        dir.removeRecursively();
        qDebug() << "已清理临时目录:" << tempDir;
    }
}
