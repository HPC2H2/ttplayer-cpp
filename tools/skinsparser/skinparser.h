/*
 * TTPlayer Skin Parser
 * 用于解析千千静听 .skn 皮肤文件（ZIP格式）和 Skin.xml 配置
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
    QString name;           // 元素名称, 如 "play", "pause", "progress"
    QRect position;          // 位置 (left, top, right, bottom)
    QString image;           // 对应的图片文件名
    QString thumbImage;      // 滑块图片（用于 slider）
    QString fillImage;       // 填充图片（用于 volume）
    QString barImage;        // 滑轨图片
    QColor color;            // 文字颜色
    QColor bkgnd;            // 背景颜色
    QString font;            // 字体名
    int fontSize = 12;       // 字体大小
    bool vertical = false;   // 是否垂直（音量滑块）
    QString align;           // 对齐方式
};

struct SkinWindow {
    QString name;                    // 窗口名称: player_window, lyric_window, ...
    QRect position;                   // 窗口位置/尺寸
    QString image;                    // 背景图片
    QColor transparentColor;          // 透明色
    QList<SkinElement> elements;      // 子元素列表
    // 歌词窗口特有
    QRect resizeRect;
    int resizeTile = 0;
    // 均衡器窗口特有
    int eqInterval = 0;
    // 播放列表窗口特有
    QString scrollbarButtonsImage;
    QString scrollbarThumbImage;
    QString scrollbarBarImage;
    int scrollbarThumbResizeCenter = 0;
    int scrollbarThumbResizeTile = 0;
};

struct SkinConfig {
    int version = 2;
    QString name;             // 皮肤名称
    QString author;           // 作者
    QString url;
    QString email;

    SkinWindow playerWindow;
    SkinWindow lyricWindow;
    SkinWindow equalizerWindow;
    SkinWindow playlistWindow;

    // 颜色方案 (来自 Visual.xml / Lyric.xml / PlayList.xml)
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

    // 辅助方法: 按名称查找元素
    const SkinElement* findElement(const QString& windowName, const QString& elemName) const;
    const SkinWindow* getWindow(const QString& windowName) const;
};

// ========== 核心解析类 ==========

class SkinParser
{
public:
    // 从解压后的目录加载皮肤配置
    static bool loadFromDirectory(const QString& dirPath, SkinConfig& outConfig);

    // 从 .skn 文件加载（自动解压到临时目录）
    static bool loadFromSknFile(const QString& sknPath, SkinConfig& outConfig, QString& outTempDir);

    // 将 .skn 文件解压到指定目录
    static bool extractSknFile(const QString& sknPath, const QString& destDir);

    // 列出 .skn 内的文件清单
    static QStringList listSknContents(const QString& sknPath);

    // 验证是否为有效的 .skn 文件
    static bool isValidSknFile(const QString& sknPath);

    // 清理解压的临时目录
    static void cleanupTempDir(const QString& tempDir);

private:
    static bool parseSkinXml(const QString& xmlPath, SkinConfig& config);
    static bool parseVisualXml(const QString& xmlPath, SkinConfig& config);
    static bool parseLyricXml(const QString& xmlPath, SkinConfig& config);
    static bool parsePlaylistXml(const QString& xmlPath, SkinConfig& config);
    static QRect parseRect(const QString& rectStr);
    static QColor parseColor(const QString& colorStr);
};

#endif // SKINPARSER_H
