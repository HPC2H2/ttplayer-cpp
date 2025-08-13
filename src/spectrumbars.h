/*
 * @author :HPC2H2
 * @date   :2025-08-12
 * @version:1.3
 * @brief  :声明SpectrumBars类，用于在音乐播放过程中显示实时频谱柱状图
 *          该类实现了音频频谱的可视化，支持实时MP3解码
 *          (Declaration of SpectrumBars class for displaying real-time
 *          spectrum visualization during music playback)
 */
#ifndef SPECTRUMBARS_H
#define SPECTRUMBARS_H

#include <QWidget>          // 基础窗口部件
#include <QMediaPlayer>     // 媒体播放器
#include <QTimer>           // 定时器
#include <QColor>           // 颜色定义
#include <QPainterPath>     // 绘制路径
#include <QPropertyAnimation> // 属性动画
#include <QPaintEvent>      // 绘制事件
#include <QTimerEvent>      // 定时器事件
#include <QFile>            // 文件操作
#include <QUrl>             // URL处理
#include <QMutex>           // 互斥锁
#include <vector>           // 标准向量容器
#include <cmath>            // 数学函数

#include "mp3decoder.h"     // MP3解码器

/**
 * @class SpectrumBars
 * @brief 频谱可视化组件，显示音频的实时频谱分析
 * 
 * SpectrumBars类提供了一个可视化的频谱分析器，可以显示音乐播放过程中的频率分布。
 * 它使用MP3Decoder类解码音频文件并获取频谱数据，然后通过柱状图的形式直观地
 * 展示不同频率的能量分布。
 */
class SpectrumBars : public QWidget
{
    Q_OBJECT
    // 使用qreal类型保证跨平台兼容性
    // peakDecay控制频谱柱峰值下降的速度，值越大下降越快
    Q_PROPERTY(qreal peakDecay READ peakDecay WRITE setPeakDecay)


public:
    /**
     * @brief 构造函数
     */
    explicit SpectrumBars(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数，负责清理资源
     */
    ~SpectrumBars();

    /**
     * @brief 定时器驱动的主更新函数（~100 FPS）
    */
    void updateFrame();

    /**
     * @brief 设置媒体播放器，用于获取音频数据和播放状态
     * @param player 媒体播放器指针
     * 
     * 设置媒体播放器后，SpectrumBars会监听播放器的状态变化，
     * 并尝试从播放的媒体文件中获取实际音频数据用于频谱分析。
     */
    void setMediaPlayer(QMediaPlayer *player);
    
    /**
     * @brief 设置频谱柱的颜色方案
     * @param topColor 频谱柱顶部颜色
     * @param bottomColor 频谱柱底部颜色
     * @param midColor 频谱柱中间颜色
     * @param peakColor 频谱峰值指示器颜色
     */
    void setColors(const QColor &topColor, const QColor &bottomColor, 
                  const QColor &midColor, const QColor &peakColor);

    /**
     * @brief 设置所有频谱柱的宽度和间距
     * @param width 每个频谱柱的宽度（像素）
     * @param spacing 频谱柱之间的间距（像素）
     */
    void setBarSize(int width, int spacing);
    
    /**
     * @brief 更新频谱显示以匹配指定的播放位置
     * @param position 播放位置（毫秒）
     * 
     * 当用户拖动进度条时，使频谱显示与新的播放位置同步。
     */
    void updateForPosition(qint64 position);

    /**
     * @brief 获取峰值衰减速度
     * @return 当前的峰值衰减速度值
     */
    qreal peakDecay() const { return m_peakDecay; }
    
    /**
     * @brief 设置峰值衰减速度
     * @param value 衰减速度值，推荐范围0.01-0.2
     * 
     * 峰值衰减速度控制频谱峰值指示器下降的速度，
     * 值越大，峰值下降越快，视觉效果越活跃。
     */
    void setPeakDecay(qreal value) { m_peakDecay = value; }

protected:
    /**
     * @brief 绘制频谱柱状图
     * @param event 绘制事件
     * 
     * 负责绘制频谱柱状图和峰值指示器，
     * 使用渐变色和抗锯齿技术提高视觉效果。
     */
    void paintEvent(QPaintEvent *event) override;
    
    /**
     * @brief 处理定时器事件，用于动画效果
     * @param event 定时器事件
     * 
     * 主要用于实现峰值指示器的平滑衰减动画效果。
     */
    void timerEvent(QTimerEvent *event) override;

private slots:
    /**
     * @brief 处理音频数据，更新频谱显示
     * 
     * 根据当前播放状态，从MP3解码器获取实际音频数据，
     * 并更新频谱显示。如果没有实际音频数据，则不更新频谱。
     */
    void processAudio();
    
    /**
     * @brief 处理播放状态变化
     * @param state 新的播放状态
     * 
     * 当播放器状态变化时调用，用于同步频谱显示状态。
     */
    void handlePlaybackStateChanged(QMediaPlayer::PlaybackState state);
    
    /**
     * @brief 更新频谱显示
     */
    void updateSpectrum();

private:
    /**
     * @brief 从当前播放的媒体文件获取音频数据
     * 
     * 使用MP3Decoder解码MP3文件并获取实时音频数据。
     * 设置回调函数以接收频谱数据更新。
     */
    void tryGetRealAudioData();
    
    // 核心组件
    QMediaPlayer *m_mediaPlayer;      // 媒体播放器指针，用于获取音频数据
    QTimer *m_updateTimer;            // 更新定时器，控制频谱刷新频率
    int m_timerId;                    // 定时器ID，用于动画效果
    
    // 频谱数据
    std::vector<float> m_spectrum;     // 存储当前频谱数据
    std::vector<float> m_peakPositions; // 存储频谱峰值位置
    
    // 动画属性
    qreal m_peakDecay;                // 峰值衰减速度
    QPropertyAnimation *m_peakAnimation; // 峰值动画
    
    // 可视化设置
    QColor m_topColor;                // 频谱柱顶部颜色
    QColor m_bottomColor;             // 频谱柱底部颜色
    QColor m_midColor;                // 频谱柱中间颜色
    QColor m_peakColor;               // 峰值指示器颜色

    // 频谱柱配置
    static constexpr int kBarsAmount = 41; // 频谱柱数量，奇数可以保证中心对称
    int m_barWidth = 3;   // 频谱柱宽度（像素）
    int m_barSpacing = 1; // 频谱柱间距（像素）

    std::vector<int> m_logMapping; // 存储每个柱子对应的最高 bin 索引
    void calculateLogFrequencyMapping();

    // 音频处理参数
    // 常量
    static const int FFT_SIZE = 1024;
    static const int MIN_FREQ = 20;
    static const int MAX_FREQ = 20000;

    // 变量
    int m_sampleRate;                 // 音频采样率（Hz）
    int m_channelCount;               // 音频通道数（1=单声道，2=立体声）
    bool m_spectrumDirty;             // 频谱数据是否已更新需要重绘
    
    // 实际音频数据
    QString m_currentFilePath;        // 当前播放文件的路径
    MP3Decoder* m_mp3Decoder;         // MP3解码器，用于解码MP3文件
    QMutex m_spectrumMutex;           // 用于保护频谱数据的互斥锁
};

#endif // SPECTRUMBARS_H
