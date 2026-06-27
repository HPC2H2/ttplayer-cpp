/*
 * TTPlayer Skin Engine - 皮肤引擎
 * 
 * 功能:
 * 1. 加载 .skn 皮肤文件或解压后的目录
 * 2. 解析 Skin.xml 获取 UI 元素布局和图片映射
 * 3. 提供统一的图片/颜色/字体查询接口
 * 4. 支持运行时热切换皮肤（拖放 .skn 即可换肤）
 * 5. 管理临时解压目录的生命周期
 */
#ifndef SKINENGINE_H
#define SKINENGINE_H

#include <QObject>
#include <QString>
#include <QPixmap>
#include <QColor>
#include <QFont>
#include <QRect>
#include <QMap>
#include <QDir>
#include <memory>

// 使用轻量级配置结构体（与 skinsparser 共享同一套数据结构定义）
// 将 skinparser.h 的核心类型直接引入
#include "skinparser.h"

class SkinEngine : public QObject
{
    Q_OBJECT

public:
    static SkinEngine& instance();

    // ========== 皮肤加载 ==========

    // 从 .skn 文件加载皮肤（自动解压到临时目录）
    bool loadSkin(const QString& sknPath);

    // 从已解压的目录加载皮肤
    bool loadFromDirectory(const QString& dirPath);

    // 重置为默认内嵌皮肤
    bool resetToDefault();

    // 卸载当前皮肤并清理资源
    void unload();

    // 检查是否有有效皮肤加载
    bool hasValidSkin() const { return m_loaded && !m_skinDir.isEmpty(); }

    // ========== 图片查询 ==========

    // 获取元素对应的主图片（带四宫格裁剪支持）
    QPixmap getElementImage(const QString& windowName, const QString& elemName) const;

    // 获取滑块缩略图
    QPixmap getThumbImage(const QString& windowName, const QString& elemName) const;

    // 获取填充图
    QPixmap getFillImage(const QString& windowName, const QString& elemName) const;

    // 获取窗口背景图
    QPixmap getWindowBackground(const QString& windowName) const;

    // 直接按文件名获取图片（从当前皮肤目录）
    QPixmap getImage(const QString& filename) const;

    // 四宫格裁剪: 返回 [正常, 悬停, 按下, 禁用]
    QList<QPixmap> cropFourStates(const QString& imageFilename) const;

    // ========== 布局查询 ==========

    // 获取元素位置
    QRect getElementRect(const QString& windowName, const QString& elemName) const;

    // 获取窗口尺寸
    QSize getWindowSize(const QString& windowName) const;

    // ========== 颜色/字体查询 ==========

    QColor getSpectrumTopColor() const;
    QColor getSpectrumBottomColor() const;
    QColor getSpectrumMidColor() const;
    QColor getSpectrumPeakColor() const;

    QColor getLyricTextColor() const;
    QColor getLyricHighlightColor() const;
    QFont getLyricFont() const;

    QColor getInfoTextColor() const;
    QFont getInfoFont() const;

    QColor getTransparent() const;

    // ========== 皮肤元信息 ==========

    QString skinName() const { return m_config.name; }
    QString skinAuthor() const { return m_config.author; }
    int skinVersion() const { return m_config.version; }

    // 当前皮肤的根目录（用于其他模块查找资源）
    QString skinRootDir() const { return m_skinDir; }

    // 是否为默认内嵌皮肤
    bool isDefaultSkin() const { return m_isDefaultSkin; }

    // 获取内部配置引用（供高级用法）
    const SkinConfig& getConfig() const { return m_config; }

signals:
    // 皮肤即将切换（旧皮肤仍可用）
    void skinAboutToChange();

    // 皮肤已完成切换
    void skinChanged(const QString& skinName);

    // 切换失败
    void skinLoadFailed(const QString& reason);

private:
    SkinEngine(QObject* parent = nullptr);
    ~SkinEngine();
    SkinEngine(const SkinEngine&) = delete;
    SkinEngine& operator=(const SkinEngine&) = delete;

    // 内部: 加载辅助 XML 配置
    bool loadAuxConfigs(SkinConfig& config, const QDir& dir);

    // 缓存清理
    void clearCache();

    // 成员变量
    SkinConfig m_config;
    QString m_skinDir;          // 当前皮肤目录路径
    QString m_tempDir;          // 临时解压目录（仅从 .skn 加载时使用）
    bool m_loaded = false;
    bool m_isDefaultSkin = true;

    // 图片缓存 (避免重复加载)
    mutable QMap<QString, QPixmap> m_imageCache;
};

#endif // SKINENGINE_H
