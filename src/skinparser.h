/*
 * TTPlayer Skin Parser (Lite / Embedded Version)
 * 
 * 轻量级皮肤配置解析器，供 ttplayer 主程序内嵌使用。
 * 不依赖任何外部库（无 QuaZip），仅使用标准 C++ 和 Qt。
 *
 * 完整版本（支持 ZIP 解压）位于: tools/skinsparser/
 */
#ifndef SKINPARSER_H
#define SKINPARSER_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <QRect>
#include <QColor>
#include <QSize>

// ========== 数据结构 ==========

struct SkinElement {
    QString name;
    QRect position;
    QString image;
    QString thumbImage;
    QString fillImage;
    QString barImage;
    QColor color;
    QColor bkgnd;
    QString font;
    int fontSize = 12;
    bool vertical = false;
    QString align;
};

struct SkinWindow {
    QString name;
    QRect position;
    QString image;
    QColor transparentColor;
    QList<SkinElement> elements;
    QRect resizeRect;
    int resizeTile = 0;
    int eqInterval = 0;
    QString scrollbarButtonsImage;
    QString scrollbarThumbImage;
    QString scrollbarBarImage;
    int scrollbarThumbResizeCenter = 0;
    int scrollbarThumbResizeTile = 0;
};

struct SkinConfig {
    int version = 2;
    QString name;
    QString author;
    QString url;
    QString email;

    QColor transparentColor = QColor("#ff00ff");

    SkinWindow playerWindow;
    SkinWindow lyricWindow;
    SkinWindow equalizerWindow;
    SkinWindow playlistWindow;

    struct {
        QColor spectrumTop;
        QColor spectrumBottom;
        QColor spectrumMid;
        QColor spectrumPeak;
        bool blurEnabled = false;
        int blurSpeed = 3;
    } visual;

    struct {
        QString fontFamily = "SimSun";
        int fontSize = -12;
        QColor textColor = QColor("#0080c0");
        QColor highlightColor = QColor("#00ff00");
        QColor backgroundColor = QColor("#000000");
    } lyric;

    struct {
        QColor textColor = QColor("#0080ff");
        QColor highlightColor = QColor("#00ff00");
        QColor backgroundColor = QColor("#000000");
        QColor alternateBackground = QColor("#202020");
        QColor selectedColor = QColor("#3269c8");
        QColor timeColor = QColor("#c08020");
    } playlist;

    const SkinElement* findElement(const QString& windowName, const QString& elemName) const;
    const SkinWindow* getWindow(const QString& windowName) const;
};

// ========== 核心解析类 ==========

class SkinParser
{
public:
    // 从解压后的目录加载皮肤配置（不需要任何外部库）
    static bool loadFromDirectory(const QString& dirPath, SkinConfig& outConfig);

    // 从 .skn 文件加载（自动解压到临时目录）- 内置最小化 ZIP 解析
    static bool loadFromSknFile(const QString& sknPath, SkinConfig& outConfig, QString& outTempDir);

    // 将 .skn 文件解压到指定目录（内置 ZIP 解析，无需 QuaZip）
    static bool extractSknFile(const QString& sknPath, const QString& destDir);

    // 列出 .skn 内的文件清单
    static QStringList listSknContents(const QString& sknPath);

    // 验证是否为有效的 .skn 文件（检查 ZIP 魔数）
    static bool isValidSknFile(const QString& sknPath);

    // 清理解压的临时目录
    static void cleanupTempDir(const QString& tempDir);

    // 解析辅助 XML（公开，供 SkinEngine 调用）
    static bool parseVisualXml(const QString& xmlPath, SkinConfig& config);
    static bool parseLyricXml(const QString& xmlPath, SkinConfig& config);
    static bool parsePlaylistXml(const QString& xmlPath, SkinConfig& config);

private:
    static bool parseSkinXml(const QString& xmlPath, SkinConfig& config);
    static QRect parseRect(const QString& rectStr);
    static QColor parseColor(const QString& colorStr);

    // 最小化 ZIP 解析（仅支持 STORE 和 DEFLATE 压缩方法）
    static bool extractZipFile(const QString& zipPath, const QString& destDir);
};

#endif // SKINPARSER_H
