/*
 * TTPlayer Skin Parser (Embedded/Lite Implementation)
 *
 * 内嵌版本 - 零外部依赖，仅使用 Qt 核心库实现 ZIP 解析。
 * 供 ttplayer 主程序使用，支持拖放 .skn 换肤功能。
 */
#include "skinparser.h"
#include <QFile>
#include <QDir>
#include <QXmlStreamReader>
#include <QTemporaryDir>
#include <QDebug>
#include <QtEndian>
#include <QDataStream>
#include <zlib.h>

// ========== SkinConfig 辅助方法 ==========

const SkinElement* SkinConfig::findElement(const QString& windowName, const QString& elemName) const
{
    const SkinWindow* win = getWindow(windowName);
    if (!win) return nullptr;
    for (const auto& elem : win->elements) {
        if (elem.name == elemName) return &elem;
    }
    return nullptr;
}

const SkinWindow* SkinConfig::getWindow(const QString& windowName) const
{
    if (windowName == "player_window" || windowName == "player") return &playerWindow;
    if (windowName == "lyric_window" || windowName == "lyric")   return &lyricWindow;
    if (windowName == "equalizer_window" || windowName == "equalizer") return &equalizerWindow;
    if (windowName == "playlist_window" || windowName == "playlist") return &playlistWindow;
    return nullptr;
}

// ========== 解析辅助函数 ==========

QRect SkinParser::parseRect(const QString& rectStr)
{
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
    if (c.startsWith('#')) return QColor(c);
    return QColor(c);
}

// ============================================================
// XML 解析（与完整版相同）
// ============================================================

bool SkinParser::parseSkinXml(const QString& xmlPath, SkinConfig& config)
{
    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[SkinParser] 无法打开 Skin.xml:" << xmlPath;
        return false;
    }

    // 读取全部内容并显式转为 UTF-8（避免 Qt6 在中文 Windows 上误用 GBK 编码）
    QByteArray rawData = file.readAll();
    file.close();
    QString content = QString::fromUtf8(rawData);

    QXmlStreamReader xml(content);
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
            } else if (name == "player_window") {
                currentWindow = &config.playerWindow;
                currentWindow->name = "player_window";
                currentWindow->image = xml.attributes().value("image").toString();
                currentWindow->transparentColor = config.transparentColor;
            } else if (name == "lyric_window") {
                currentWindow = &config.lyricWindow;
                currentWindow->name = "lyric_window";
                currentWindow->image = xml.attributes().value("image").toString();
                currentWindow->position = parseRect(xml.attributes().value("position").toString());
                QString rr = xml.attributes().value("resize_rect").toString();
                if (!rr.isEmpty()) currentWindow->resizeRect = parseRect(rr);
                currentWindow->resizeTile = xml.attributes().value("resize_tile").toString().toInt();
            } else if (name == "equalizer_window") {
                currentWindow = &config.equalizerWindow;
                currentWindow->name = "equalizer_window";
                currentWindow->image = xml.attributes().value("image").toString();
                currentWindow->position = parseRect(xml.attributes().value("position").toString());
                currentWindow->eqInterval = xml.attributes().value("eq_interval").toString().toInt();
            } else if (name == "playlist_window") {
                currentWindow = &config.playlistWindow;
                currentWindow->name = "playlist_window";
                currentWindow->image = xml.attributes().value("image").toString();
                currentWindow->position = parseRect(xml.attributes().value("position").toString());
                QString rr = xml.attributes().value("resize_rect").toString();
                if (!rr.isEmpty()) currentWindow->resizeRect = parseRect(rr);
                currentWindow->resizeTile = xml.attributes().value("resize_tile").toString().toInt();
            } else if (currentWindow && name != "skin") {
                SkinElement elem;
                elem.name = name;
                QXmlStreamAttributes attrs = xml.attributes();

                if (attrs.hasAttribute("position")) elem.position = parseRect(attrs.value("position").toString());
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

    bool ok = !xml.hasError();
    if (!ok) qWarning() << "[SkinParser] XML 解析错误:" << xml.errorString();
    file.close();

    if (ok) {
        qDebug() << "[SkinParser] 成功解析皮肤:" << config.name
                 << "- player 元素数:" << config.playerWindow.elements.size();
    }

    return ok;
}

bool SkinParser::parseVisualXml(const QString& xmlPath, SkinConfig& config)
{
    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QByteArray rawData = file.readAll();
    file.close();
    QXmlStreamReader xml(QString::fromUtf8(rawData));
    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement()) {
            auto a = xml.attributes();
            QString n = xml.name().toString();
            if (n == "visual" || n == "spectrum") {
                if (a.hasAttribute("top_color"))     config.visual.spectrumTop     = parseColor(a.value("top_color").toString());
                if (a.hasAttribute("bottom_color"))  config.visual.spectrumBottom  = parseColor(a.value("bottom_color").toString());
                if (a.hasAttribute("mid_color"))     config.visual.spectrumMid     = parseColor(a.value("mid_color").toString());
                if (a.hasAttribute("peak_color"))    config.visual.spectrumPeak    = parseColor(a.value("peak_color").toString());
                if (a.hasAttribute("blur"))          config.visual.blurEnabled    = (a.value("blur").toString() == "1");
                if (a.hasAttribute("blur_speed"))    config.visual.blurSpeed      = a.value("blur_speed").toString().toInt();
            }
        }
    }
    file.close();
    return true;
}

bool SkinParser::parseLyricXml(const QString& xmlPath, SkinConfig& config)
{
    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QByteArray rawData = file.readAll();
    file.close();
    QXmlStreamReader xml(QString::fromUtf8(rawData));
    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement()) {
            auto a = xml.attributes();
            QString n = xml.name().toString();
            if (n == "font" || n == "lyric") {
                if (a.hasAttribute("face"))  config.lyric.fontFamily = a.value("face").toString();
                if (a.hasAttribute("family")) config.lyric.fontFamily = a.value("family").toString();
                if (a.hasAttribute("size"))  config.lyric.fontSize = a.value("size").toString().toInt();
            }
            if (n == "color" || n == "text") {
                if (a.hasAttribute("text"))      config.lyric.textColor      = parseColor(a.value("text").toString());
                if (a.hasAttribute("highlight")) config.lyric.highlightColor = parseColor(a.value("highlight").toString());
            }
            if (n == "background") {
                if (a.hasAttribute("color")) config.lyric.backgroundColor = parseColor(a.value("color").toString());
            }
        }
    }
    file.close();
    return true;
}

bool SkinParser::parsePlaylistXml(const QString& xmlPath, SkinConfig& config)
{
    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QByteArray rawData = file.readAll();
    file.close();
    QXmlStreamReader xml(QString::fromUtf8(rawData));
    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement()) {
            auto a = xml.attributes();
            QString n = xml.name().toString();
            if (n == "color" || n == "text") {
                if (a.hasAttribute("normal") || a.hasAttribute("text"))
                    config.playlist.textColor = parseColor(
                        a.hasAttribute("normal") ? a.value("normal").toString() : a.value("text").toString());
                if (a.hasAttribute("highlight"))
                    config.playlist.highlightColor = parseColor(a.value("highlight").toString());
                if (a.hasAttribute("time"))
                    config.playlist.timeColor = parseColor(a.value("time").toString());
            }
            if (n == "background") {
                if (a.hasAttribute("color") || a.hasAttribute("normal"))
                    config.playlist.backgroundColor = parseColor(
                        a.hasAttribute("color") ? a.value("color").toString() : a.value("normal").toString());
                if (a.hasAttribute("alternate"))
                    config.playlist.alternateBackground = parseColor(a.value("alternate").toString());
            }
        }
    }
    file.close();
    return true;
}

// ============================================================
// 最小化 ZIP 解析实现
//
// ZIP Local File Header 结构 (30 bytes):
//   0  : signature (0x04034b50) - 4 bytes
//   4  : version needed         - 2 bytes
//   6  : general purpose flag   - 2 bytes
//   8  : compression method     - 2 bytes (0=STORE, 8=DEFLATE)
//   10 : last mod time          - 2 bytes
//  12  : last mod date           - 2 bytes
//   14 : CRC-32                  - 4 bytes
//   18 : compressed size         - 4 bytes
//   22 : uncompressed size       - 4 bytes
//   26 : file name length        - 2 bytes
//   28 : extra field length      - 2 bytes
//   30 : file name               (variable)
//   ...: extra field             (variable)
//   ...: file data                (variable)
// ============================================================

namespace {

#pragma pack(push, 1)
struct ZipLocalHeader {
    uint32_t signature;    // 0x04034b50
    uint16_t versionNeeded;
    uint16_t flags;
    uint16_t compressionMethod; // 0=store, 8=deflate
    uint16_t lastModTime;
    uint16_t lastModDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t fileNameLength;
    uint16_t extraFieldLength;
};
#pragma pack(pop)

} // anonymous namespace

bool SkinParser::extractZipFile(const QString& zipPath, const QString& destDir)
{
    QFile zipFile(zipPath);
    if (!zipFile.open(QIODevice::ReadOnly)) {
        qWarning() << "[SkinParser] 无法打开 ZIP 文件:" << zipPath;
        return false;
    }

    QDir dest(destDir);
    if (!dest.exists())
        dest.mkpath(".");

    int fileCount = 0;

    while (!zipFile.atEnd()) {
        ZipLocalHeader header;
        qint64 bytesRead = zipFile.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (bytesRead != sizeof(header))
            break;

        // 检查签名
        if (qFromLittleEndian(header.signature) != 0x04034b50) {
            break; // 到达非 local header 区域（可能是中央目录等）
        }

        uint16_t fileNameLen = qFromLittleEndian(header.fileNameLength);
        uint16_t extraLen    = qFromLittleEndian(header.extraFieldLength);
        uint32_t compSize   = qFromLittleEndian(header.compressedSize);
        uint32_t uncompSize = qFromLittleEndian(header.uncompressedSize);
        uint16_t compMethod = qFromLittleEndian(header.compressionMethod);

        // 读取文件名
        QByteArray fileNameBytes = zipFile.read(fileNameLen);
        QString fileName = QString::fromUtf8(fileNameBytes);

        // 跳过额外字段
        if (extraLen > 0)
            zipFile.skip(extraLen);

        // 跳过目录条目
        if (fileName.endsWith('/')) {
            dest.mkpath(fileName);
            continue;
        }

        // 创建目标文件的目录
        QString fullPath = dest.filePath(fileName);
        QFileInfo fileInfo(fullPath);
        fileInfo.absoluteDir().mkpath(".");

        // 读取文件数据
        QByteArray compressedData = zipFile.read(compSize);

        if (compMethod == 0) {
            // STORE 方法（无压缩）
            QFile outFile(fullPath);
            if (outFile.open(QIODevice::WriteOnly)) {
                outFile.write(compressedData);
                outFile.close();
                fileCount++;
            }
        } else if (compMethod == 8) {
            // DEFLATE 方法 - 使用 zlib raw inflate 解压
            QFile outFile(fullPath);
            if (outFile.open(QIODevice::WriteOnly)) {
                bool success = false;

                // 使用 zlib 的 raw inflate (窗口值 -15 表示无 zlib header)
                z_stream strm;
                memset(&strm, 0, sizeof(strm));
                // 负的窗口值 = raw inflate（跳过 zlib header）
                int ret = inflateInit2(&strm, -MAX_WBITS);
                if (ret == Z_OK) {
                    strm.avail_in = static_cast<uInt>(compressedData.size());
                    strm.next_in = reinterpret_cast<Bytef*>(compressedData.data());

                    // 输出缓冲区
                    QByteArray outBuf;
                    outBuf.resize(uncompSize > 0 ? uncompSize : compSize * 4);
                    strm.avail_out = static_cast<uInt>(outBuf.size());
                    strm.next_out = reinterpret_cast<Bytef*>(outBuf.data());

                    ret = inflate(&strm, Z_FINISH);
                    if (ret == Z_STREAM_END || ret == Z_OK) {
                        outBuf.resize(strm.total_out);
                        outFile.write(outBuf);
                        success = true;
                    } else {
                        qWarning() << "[SkinParser] inflate 失败:" << fileName
                                   << "error:" << ret << "msg:" << (strm.msg ? strm.msg : "");
                    }
                    inflateEnd(&strm);
                }

                if (!success) {
                    qWarning() << "[SkinParser] 无法 DEFLATE 解压:" << fileName << "(尝试原样保存)";
                    outFile.write(compressedData);
                }
                outFile.close();
                fileCount++;
            }
        } else {
            qWarning() << "[SkinParser] 不支持的压缩方法:" << compMethod << "for" << fileName;
        }

        // 对齐到下一个 local header（某些情况下有 data descriptor）
        // 简单处理：如果设置了 bit 3，则跳过 data descriptor (12 or 16 bytes)
        uint16_t flags = qFromLittleEndian(header.flags);
        if (flags & 0x08) {
            // Data descriptor follows (可选签名 0x08074b50)
            char peekBuf[4];
            if (zipFile.peek(peekBuf, 4) == 4) {
                uint32_t peekSig = qFromLittleEndian(*reinterpret_cast<uint32_t*>(peekBuf));
                if (peekSig == 0x08074b50) {
                    zipFile.skip(16); // signature + data descriptor
                } else {
                    zipFile.skip(12); // data descriptor without signature
                }
            }
        }
    }

    zipFile.close();
    qDebug() << "[SkinParser] ZIP 解压完成:" << zipPath << "->" << destDir
             << "(" << fileCount << "个文件)";
    return true;
}

// ============================================================
// 核心 API 实现
// ============================================================

bool SkinParser::loadFromDirectory(const QString& dirPath, SkinConfig& outConfig)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        qWarning() << "[SkinParser] 目录不存在:" << dirPath;
        return false;
    }

    QString skinXmlPath = dir.filePath("Skin.xml");
    if (!QFile::exists(skinXmlPath)) {
        qWarning() << "[SkinParser] 未找到 Skin.xml:" << skinXmlPath;
        return false;
    }

    if (!parseSkinXml(skinXmlPath, outConfig))
        return false;

    // 加载子配置
    QString visualXmlPath = dir.filePath("Visual.xml");
    if (QFile::exists(visualXmlPath)) parseVisualXml(visualXmlPath, outConfig);

    QString lyricXmlPath = dir.filePath("Lyric.xml");
    if (QFile::exists(lyricXmlPath)) parseLyricXml(lyricXmlPath, outConfig);

    QString playlistXmlPath = dir.filePath("PlayList.xml");
    if (QFile::exists(playlistXmlPath)) parsePlaylistXml(playlistXmlPath, outConfig);

    qDebug() << "[SkinParser] 从目录加载皮肤成功:" << outConfig.name;
    return true;
}

bool SkinParser::extractSknFile(const QString& sknPath, const QString& destDir)
{
    if (!isValidSknFile(sknPath))
        return false;

    return extractZipFile(sknPath, destDir);
}

bool SkinParser::loadFromSknFile(const QString& sknPath, SkinConfig& outConfig, QString& outTempDir)
{
    if (!isValidSknFile(sknPath))
        return false;

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        qWarning() << "[SkinParser] 无法创建临时目录";
        return false;
    }
    tempDir.setAutoRemove(false);
    outTempDir = tempDir.path();

    if (!extractSknFile(sknPath, outTempDir))
        return false;

    return loadFromDirectory(outTempDir, outConfig);
}

QStringList SkinParser::listSknContents(const QString& sknPath)
{
    QStringList contents;
    if (!isValidSknFile(sknPath))
        return contents;

    QFile zipFile(sknPath);
    if (!zipFile.open(QIODevice::ReadOnly))
        return contents;

    while (!zipFile.atEnd()) {
        ZipLocalHeader header;
        if (zipFile.read(reinterpret_cast<char*>(&header), sizeof(header)) != sizeof(header))
            break;

        if (qFromLittleEndian(header.signature) != 0x04034b50)
            break;

        uint16_t fnLen = qFromLittleEndian(header.fileNameLength);
        uint16_t exLen = qFromLittleEndian(header.extraFieldLength);
        uint32_t cSz  = qFromLittleEndian(header.compressedSize);

        QByteArray fn = zipFile.read(fnLen);
        contents.append(QString::fromUtf8(fn));

        zipFile.seek(zipFile.pos() + exLen + cSz);

        // 跳过 data descriptor 如果存在
        uint16_t flags = qFromLittleEndian(header.flags);
        if (flags & 0x08) {
            char peek[4];
            if (zipFile.peek(peek, 4) == 4) {
                if (qFromLittleEndian(*reinterpret_cast<uint32_t*>(peek)) == 0x08074b50)
                    zipFile.skip(16);
                else
                    zipFile.skip(12);
            }
        }
    }

    zipFile.close();
    return contents;
}

bool SkinParser::isValidSknFile(const QString& sknPath)
{
    QFile file(sknPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray header = file.read(4);
    file.close();

    bool valid = (header.size() >= 4 &&
                  static_cast<unsigned char>(header[0]) == 0x50 &&
                  static_cast<unsigned char>(header[1]) == 0x4B &&
                  static_cast<unsigned char>(header[2]) == 0x03 &&
                  static_cast<unsigned char>(header[3]) == 0x04);

    return valid;
}

void SkinParser::cleanupTempDir(const QString& tempDir)
{
    if (tempDir.isEmpty()) return;
    QDir dir(tempDir);
    if (dir.exists()) {
        dir.removeRecursively();
        qDebug() << "[SkinParser] 已清理临时目录:" << tempDir;
    }
}
