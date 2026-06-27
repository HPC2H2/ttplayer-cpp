/*
 * TTPlayer Skin Engine 实现
 */
#include "skinengine.h"
#include "skinparser.h"
#include <QFile>
#include <QDir>
#include <QPixmapCache>
#include <QDebug>

// ========== 单例 ==========

SkinEngine& SkinEngine::instance()
{
    static SkinEngine inst;
    return inst;
}

SkinEngine::SkinEngine(QObject* parent)
    : QObject(parent)
{
}

SkinEngine::~SkinEngine()
{
    unload();
}

// ========== 皮肤加载/卸载 ==========

bool SkinEngine::loadSkin(const QString& sknPath)
{
    if (!SkinParser::isValidSknFile(sknPath)) {
        emit skinLoadFailed(QString("无效的 .skn 文件: %1").arg(sknPath));
        return false;
    }

    emit skinAboutToChange();

    // 先清理旧资源
    unload();

    // 解压到临时目录
    QString tempDir;
    SkinConfig newConfig;

    if (!SkinParser::loadFromSknFile(sknPath, newConfig, tempDir)) {
        emit skinLoadFailed("无法解析 .skn 文件中的皮肤配置");
        return false;
    }

    m_config = newConfig;
    m_skinDir = tempDir;
    m_tempDir = tempDir;
    m_loaded = true;
    m_isDefaultSkin = false;
    clearCache();

    qDebug() << "[SkinEngine] 成功加载皮肤:" << m_config.name
             << "from" << QFileInfo(sknPath).fileName()
             << "- 目录:" << m_skinDir;

    emit skinChanged(m_config.name);
    return true;
}

bool SkinEngine::loadFromDirectory(const QString& dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        emit skinLoadFailed(QString("目录不存在: %1").arg(dirPath));
        return false;
    }

    emit skinAboutToChange();
    unload();

    SkinConfig newConfig;
    if (!SkinParser::loadFromDirectory(dirPath, newConfig)) {
        emit skinLoadFailed("无法从目录加载皮肤配置 (缺少 Skin.xml?)");
        return false;
    }

    loadAuxConfigs(newConfig, dir);

    m_config = newConfig;
    m_skinDir = dir.absolutePath();
    m_tempDir.clear(); // 不是临时目录，不需要自动清理
    m_loaded = true;
    m_isDefaultSkin = false;
    clearCache();

    qDebug() << "[SkinEngine] 成功从目录加载皮肤:" << m_config.name
             << "-" << m_skinDir;

    emit skinChanged(m_config.name);
    return true;
}

bool SkinEngine::resetToDefault()
{
    emit skinAboutToChange();
    unload();

    // 默认皮肤使用 Qt 资源系统中的 Purple 皮肤
    // 不需要实际的文件目录，通过特殊标记识别
    m_skinDir = ":/skin/Purple";
    m_isDefaultSkin = true;
    m_loaded = true;

    // 为默认皮肤设置基本配置
    m_config = SkinConfig(); // 清空
    m_config.name = "Purple (默认)";
    m_config.author = "TTPlayer Team";
    m_config.version = 1;
    m_config.transparentColor = QColor("#ff00ff");

    // 默认颜色方案
    m_config.visual.spectrumTop = QColor("#8CEFFD");
    m_config.visual.spectrumBottom = QColor("#71CDFD");
    m_config.visual.spectrumMid = QColor("#4C5FD1");
    m_config.visual.spectrumPeak = QColor("#FF71CD");

    clearCache();

    qDebug() << "[SkinEngine] 已重置为默认皮肤 (Purple)";
    emit skinChanged(m_config.name);
    return true;
}

void SkinEngine::unload()
{
    if (!m_tempDir.isEmpty()) {
        SkinParser::cleanupTempDir(m_tempDir);
        m_tempDir.clear();
    }
    m_skinDir.clear();
    m_loaded = false;
    m_isDefaultSkin = true;
    clearCache();
}

// ========== 图片查询 ==========

QPixmap SkinEngine::getImage(const QString& filename) const
{
    if (!m_loaded) return QPixmap();

    // 检查缓存
    auto it = m_imageCache.find(filename);
    if (it != m_imageCache.end()) {
        return *it;
    }

    QPixmap pixmap;
    
    if (m_isDefaultSkin || m_skinDir.startsWith(":/")) {
        // 从 Qt 资源加载
        pixmap.load(m_skinDir + "/" + filename);
    } else {
        // 从文件系统加载
        QString fullPath = QDir(m_skinDir).filePath(filename);
        
        // 尝试多种大小写（Windows 不区分大小写，Linux 区分）
        if (!pixmap.load(fullPath)) {
            QDir dir(m_skinDir);
            QStringList entries = dir.entryList(QDir::Files);
            for (const QString& entry : entries) {
                if (entry.compare(filename, Qt::CaseInsensitive) == 0) {
                    pixmap.load(dir.filePath(entry));
                    break;
                }
            }
        }
    }

    if (pixmap.isNull()) {
        qWarning() << "[SkinEngine] 无法加载图片:" << filename
                   << "(搜索路径:" << m_skinDir << ")";
    } else {
        m_imageCache[filename] = pixmap;
    }

    return pixmap;
}

QPixmap SkinEngine::getElementImage(const QString& windowName, const QString& elemName) const
{
    const SkinElement* elem = m_config.findElement(windowName, elemName);
    if (elem && !elem->image.isEmpty()) {
        return getImage(elem->image);
    }
    return QPixmap();
}

QPixmap SkinEngine::getThumbImage(const QString& windowName, const QString& elemName) const
{
    const SkinElement* elem = m_config.findElement(windowName, elemName);
    if (elem && !elem->thumbImage.isEmpty()) {
        return getImage(elem->thumbImage);
    }
    // 回退到主图片
    return getElementImage(windowName, elemName);
}

QPixmap SkinEngine::getFillImage(const QString& windowName, const QString& elemName) const
{
    const SkinElement* elem = m_config.findElement(windowName, elemName);
    if (elem && !elem->fillImage.isEmpty()) {
        return getImage(elem->fillImage);
    }
    return QPixmap();
}

QPixmap SkinEngine::getWindowBackground(const QString& windowName) const
{
    const SkinWindow* win = m_config.getWindow(windowName);
    if (win && !win->image.isEmpty()) {
        return getImage(win->image);
    }
    return QPixmap();
}

QList<QPixmap> SkinEngine::cropFourStates(const QString& imageFilename) const
{
    QList<QPixmap> result;
    QPixmap original = getImage(imageFilename);
    
    if (original.isNull())
        return result;

    int w = original.width();
    int h = original.height();
    int partW = w / 4;

    for (int i = 0; i < 4; ++i) {
        result.append(original.copy(i * partW, 0, partW, h));
    }

    return result;
}

// ========== 布局查询 ==========

QRect SkinEngine::getElementRect(const QString& windowName, const QString& elemName) const
{
    const SkinElement* elem = m_config.findElement(windowName, elemName);
    if (elem)
        return elem->position;
    return QRect();
}

QSize SkinEngine::getWindowSize(const QString& windowName) const
{
    const SkinWindow* win = m_config.getWindow(windowName);
    if (win && !win->position.isEmpty()) {
        return QSize(win->position.width(), win->position.height());
    }
    // 尝试从背景图获取尺寸
    QPixmap bg = getWindowBackground(windowName);
    if (!bg.isNull())
        return bg.size();
    return QSize(276, 166); // 默认尺寸
}

// ========== 颜色/字体查询 ==========

QColor SkinEngine::getSpectrumTopColor() const { return m_config.visual.spectrumTop; }
QColor SkinEngine::getSpectrumBottomColor() const { return m_config.visual.spectrumBottom; }
QColor SkinEngine::getSpectrumMidColor() const { return m_config.visual.spectrumMid; }
QColor SkinEngine::getSpectrumPeakColor() const { return m_config.visual.spectrumPeak; }

QColor SkinEngine::getLyricTextColor() const { return m_config.lyric.textColor; }
QColor SkinEngine::getLyricHighlightColor() const { return m_config.lyric.highlightColor; }

QFont SkinEngine::getLyricFont() const
{
    QFont font(m_config.lyric.fontFamily.isEmpty() ? QFont().family() : m_config.lyric.fontFamily);
    font.setPixelSize(qAbs(m_config.lyric.fontSize)); // fontSize 可能为负值（如 -12）
    return font;
}

QColor SkinEngine::getInfoTextColor() const
{
    const SkinElement* info = m_config.findElement("player_window", "info");
    if (info && info->color.isValid())
        return info->color;
    return QColor("#ffff06"); // 默认黄色
}

QFont SkinEngine::getInfoFont() const
{
    const SkinElement* info = m_config.findElement("player_window", "info");
    QFont font(info->font.isEmpty() ? "SimSun" : info->font);
    font.setPixelSize((info->fontSize > 0) ? info->fontSize : 12);
    return font;
}

QColor SkinEngine::getTransparent() const
{
    return m_config.transparentColor;
}

// ========== 内部辅助 ==========

bool SkinEngine::loadAuxConfigs(SkinConfig& config, const QDir& dir)
{
    // 尝试解析 Visual.xml / Lyric.xml / PlayList.xml
    QString visualXml = dir.filePath("Visual.xml");
    if (QFile::exists(visualXml))
        SkinParser::parseVisualXml(visualXml, config);

    QString lyricXml = dir.filePath("Lyric.xml");
    if (QFile::exists(lyricXml))
        SkinParser::parseLyricXml(lyricXml, config);

    QString playlistXml = dir.filePath("PlayList.xml");
    if (QFile::exists(playlistXml))
        SkinParser::parsePlaylistXml(playlistXml, config);

    return true;
}

void SkinEngine::clearCache()
{
    m_imageCache.clear();
}
