/*
 * @author :HPC2H2
 * @date   :2025-08-13
 * @version:2.0 (Skin Engine 支持)
 * @brief  :主窗口类，支持动态换肤（拖放 .skn 文件即可切换）
 */
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#ifdef QT_MULTIMEDIA_ENABLED
#include <QMediaPlayer>
#else
#include "audioplayer.h"
#endif
#include <QShortcut>
#include <QPoint>
#include <QPropertyAnimation>
#include <QPixmap>
#include <QMap>

#include "fadinglabel.h"
#include "imageslider.h"
#include "spectrumbars.h"
#include "skinengine.h"

class PlayList;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void exitAll();
    void winFixed();
    void minimizeWindow();
    void showMusicList();
    void playAudio();
    void updateSliderPosition(qint64 position);
    void setSliderDuration(qint64 duration);
    void sliderPressed();
    void sliderReleased();
    void increaseVolume();
    void decreaseVolume();

    // 皮肤相关槽函数
    void onSkinChanged(const QString& skinName);

    // 供 PlayList 通过 invokeMethod 调用
    Q_INVOKABLE void setupHoverPressedIcon(QPushButton *button, const QPixmap &normalPixmap,
                                          const QPixmap &hoverPixmap, const QPixmap &pressedPixmap);

private:
    // UI 初始化与重建
    void initUI();
    void applySkin();              // 根据当前皮肤配置重建所有 UI 元素
    void rebuildFromSkinConfig();  // 从 SkinEngine 的配置数据构建布局
    void applyDefaultSkinUI();     // 回退到默认 Purple 皮肤硬编码路径

    // 辅助方法
    QPropertyAnimation* startAnimation(float start, float end);
    QList<QPixmap> cropImageIntoFourHorizontal(const QString &imagePath);
    QPixmap roundPixmap(const QPixmap &pixmap, int radius);
    void addPlaylist(const QString &filePath);

    // 播放按钮图标切换
    void switchToPauseIcon();
    void switchToPlayIcon();

    // 按图片名获取四宫格裁剪结果（优先从皮肤引擎，回退到资源系统）
    QList<QPixmap> loadButtonImages(const QString& filename);

    // UI Elements
    QPushButton *m_musicListBtn;
    QPushButton *m_previewBtn;   // 上一首 (prev)
    QPushButton *m_playBtn;
    QPushButton *m_nextBtn;
    QPushButton *m_fixedBtn;     // 置顶 (ontop)
    QPushButton *m_miniTopBtn;   // 迷你模式 (minimode)
    QPushButton *m_minBtn;
    QPushButton *m_closeBtn;
    QPushButton *m_lrcBtn;       // 歌词按钮

    ImageSlider *m_progressSlider;
    ImageSlider *m_volumeSlider;
    FadingLabel *m_currentLyricLabel;
    SpectrumBars *m_spectrumBars;

    // Player components
#ifdef QT_MULTIMEDIA_ENABLED
    QMediaPlayer *m_player;
#else
    AudioPlayer *m_audioPlayer;   // Windows 原生音频播放器
#endif
    QList<QString> m_playlist;
    int m_currentIndex;
    bool m_shuffleMode;
    QString m_currentPlayingPath;

    // Drag support
    bool m_dragging;
    QPoint m_offset;

    // Playlist window
    PlayList *m_playlistWindow;

    // Shortcuts
    QShortcut *m_spaceShortcut;
    QShortcut *m_upShortcut;
    QShortcut *m_downShortcut;

    // Animation
    QPropertyAnimation *m_animation;

    // 当前是否使用了外部皮肤（非默认）
    bool m_usingExternalSkin = false;
};

#endif // MAINWINDOW_H
