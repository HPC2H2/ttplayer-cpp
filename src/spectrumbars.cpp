/*
 * @author :HPC2H2
 * @date   :2025-08-12
 * @version:1.2
 * @brief  :SpectrumBars类的实现，用于在音乐播放过程中显示实时频谱柱状图
 *          该类实现了音频频谱的可视化，支持实时MP3解码。
 *          (Implementation of SpectrumBars class for displaying real-time
 *          spectrum visualization during music playback)
 */
#include "spectrumbars.h"
#include <QPainter>
#include <QPainterPath>
#include <QAudioBuffer>
#include <QAudioOutput>
#include <QRandomGenerator>
#include <QtMath>
#include <QDebug>
#include <cmath>


/**
 * @brief 构造函数
 */
SpectrumBars::SpectrumBars(QWidget *parent)
    : QWidget(parent),
      m_mediaPlayer(nullptr),
      m_updateTimer(new QTimer(this)),
      m_timerId(0),
      m_peakDecay(0.05),
      m_peakAnimation(new QPropertyAnimation(this, "peakDecay")),
      m_sampleRate(44100),
      m_channelCount(2),
      m_spectrumDirty(false),
      m_mp3Decoder(new MP3Decoder(this))
{
    // 设置默认颜色
    m_topColor = QColor("#8CEFFD");
    m_bottomColor = QColor("#71CDFD");
    m_midColor = QColor("#4C5FD1");
    m_peakColor = QColor("#FF71CD");
    
    // 初始化频谱数据数组
    m_spectrum.resize(kBarsAmount, 0.0f);
    m_peakPositions.resize(kBarsAmount, 0.0f);
    
    // 设置峰值衰减动画
    m_peakAnimation->setDuration(200);       // 动画持续200毫秒
    m_peakAnimation->setStartValue(0.03);    // 起始值（中等衰减）
    m_peakAnimation->setEndValue(0.25);      // 结束值（快衰减）
    m_peakAnimation->setLoopCount(-1);       // 无限循环
    m_peakAnimation->start();
    
    // 启动动画定时器
    m_timerId = startTimer(16);
    
    // 确保部件可见
    setVisible(true);
    raise();
    
    // 设置大小策略
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(40);
    
    //
    calculateLogFrequencyMapping();

    // 输出调试信息
    qDebug("SpectrumBars initialized with size: %d x %d", width(), height());
}

SpectrumBars::~SpectrumBars()
{
    if (m_timerId) {
        killTimer(m_timerId);
    }
    
    if (m_mp3Decoder) {
        m_mp3Decoder->stopDecoding();
        m_mp3Decoder->wait();
    }
}

/**
 * @brief 设置媒体播放器，
 */
void SpectrumBars::setMediaPlayer(QMediaPlayer *player)
{
    m_mediaPlayer = player;
    
    if (m_mediaPlayer) {
        // 在用户切换歌曲或媒体源变化情况下触发
        connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia) {
                
                // 获取音频文件路径
                if (m_mediaPlayer->source().isLocalFile()) {
                    m_currentFilePath = m_mediaPlayer->source().toLocalFile();
                    tryGetRealAudioData();
                }
            }
        });
        
        // 监听媒体源变化
        connect(m_mediaPlayer, &QMediaPlayer::sourceChanged, this, [this](const QUrl &source) {
            if (source.isLocalFile()) {
                m_currentFilePath = source.toLocalFile();
                tryGetRealAudioData();
            } else {
                m_currentFilePath.clear();
            }
        });
        
        // 监听播放状态变化，用于同步频谱显示状态
        connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, this, &SpectrumBars::handlePlaybackStateChanged);
        
        connect(m_updateTimer, &QTimer::timeout, this, &SpectrumBars::updateFrame);
        m_updateTimer->start(10); // 每10秒调用一次updateFrame
    }
}

/**
 * @brief 处理播放状态变化
 * @param state 新的播放状态
 * 
 * 当播放器状态变化时调用，用于同步频谱显示状态。
 */
void SpectrumBars::handlePlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    // 根据播放状态控制频谱更新
    if (state == QMediaPlayer::PlayingState) {
        // 播放状态：启用频谱更新
        if (!m_updateTimer->isActive()) {
            m_updateTimer->start(10);
        }
        
        // 重新启动动画定时器（如果已停止）
        if (m_timerId == 0) {
            m_timerId = startTimer(16);
        }
    } else {
        // 暂停状态：更新一次频谱以匹配当前位置
        if (m_mediaPlayer) {
            updateForPosition(m_mediaPlayer->position());
        }
        
        // 暂停或停止状态：暂停频谱更新
        if (m_updateTimer->isActive()) {
            m_updateTimer->stop();
            
            // 清空频谱数据，使其静止
            QMutexLocker locker(&m_spectrumMutex);
            for (int i = 0; i < kBarsAmount; ++i) {
                m_spectrum[i] *= 0.5f; // 降低但不完全清零，保留一些视觉效果
                m_peakPositions[i] = m_spectrum[i]; // 立即将峰值设置为当前值，防止继续动画
            }
            update();
        }
        
        // 停止动画定时器
        if (m_timerId) {
            killTimer(m_timerId);
            m_timerId = 0;
        }
    }
}

/**
 * @brief 从当前播放的媒体文件获取音频数据
 * 
 * 使用MP3Decoder解码MP3文件并获取实时音频数据。
 * 设置回调函数以接收频谱数据更新。
 */
void SpectrumBars::tryGetRealAudioData()
{
    if (m_currentFilePath.isEmpty()) {
        return;
    }
    
    QFile file(m_currentFilePath);
    if (!file.exists()) {
        qWarning() << "音频文件不存在:" << m_currentFilePath;
        return;
    }
    
    if (m_mp3Decoder->openFile(m_currentFilePath)) {
        m_mp3Decoder->setSpectrumCallback([this](const std::vector<float>& rawSpectrum) {
            QMutexLocker locker(&m_spectrumMutex);

            // 清空当前频谱
            std::fill(m_spectrum.begin(), m_spectrum.end(), 0.0f);

            // 执行对数频带映射
            for (int bar = 0; bar < kBarsAmount; ++bar) {
                int startBin = (bar == 0) ? 1 : m_logMapping[bar - 1];
                int endBin = m_logMapping[bar];

                float maxAmplitude = 0.0f;
                for (int bin = startBin; bin < endBin && bin < rawSpectrum.size(); ++bin) {
                    maxAmplitude = qMax(maxAmplitude, rawSpectrum[bin]);
                }

                m_spectrum[bar] = maxAmplitude;

                // 更新峰值
                if (m_spectrum[bar] > m_peakPositions[bar]) {
                    m_peakPositions[bar] = m_spectrum[bar];
                } else {
                    m_peakPositions[bar] -= m_peakDecay;
                    m_peakPositions[bar] = qMax(m_peakPositions[bar], m_spectrum[bar]);
                }
            }

            m_spectrumDirty = true;
            update();
        });

        // 媒体加载完成后，设置获取的音频参数
        m_sampleRate = m_mp3Decoder->sampleRate();
        m_channelCount = m_mp3Decoder->channels();

        qDebug() << "成功启用实时MP3解码:" << m_currentFilePath;
    } else {
        qWarning() << "无法解码MP3文件:" << m_currentFilePath;
    }
}

void SpectrumBars::setColors(const QColor &topColor, const QColor &bottomColor,
                               const QColor &midColor, const QColor &peakColor)
{
    m_topColor = topColor;
    m_bottomColor = bottomColor;
    m_midColor = midColor;
    m_peakColor = peakColor;
    update();
}

// 实现setBarSize方法，允许自定义频谱柱的宽度和间距
void SpectrumBars::setBarSize(int width, int spacing)
{
    if (width <= 0 || spacing < 0) {
        qWarning("SpectrumBars::setBarSize: Invalid bar width or spacing");
        return;
    }
    
    m_barWidth = width;
    m_barSpacing = spacing;
    
    update();
}

/**
 * @brief 更新频谱显示以匹配指定的播放位置
 * @param position 播放位置（毫秒）
 * 
 * 当用户拖动进度条时，使频谱显示与新的播放位置同步。
 */
void SpectrumBars::updateForPosition(qint64 position)
{
    if (!m_mp3Decoder || !m_mediaPlayer) {
        return;
    }
    
    // 更新解码器位置
    m_mp3Decoder->setPosition(position);

    // 如果播放器处于播放状态，确保更新定时器正在运行
    if (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
        if (!m_updateTimer->isActive()) {
            m_updateTimer->start(10);
        }
        
        // 确保动画定时器正在运行
        if (m_timerId == 0) {
            m_timerId = startTimer(16);
        }
    }
    
    // 强制处理一次音频数据，立即更新频谱
    processAudio();
    update();
}


/**
 * @brief 处理音频数据，更新频谱显示
 */
void SpectrumBars::processAudio()
{
    // 检查是否正在播放
    bool isPlaying = m_mediaPlayer && m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState;
    
    // 如果正在播放且MP3解码器已实例化，更新解码器的位置
    if (isPlaying && m_mp3Decoder) {
        static qint64 lastPosition = 0;
        qint64 position = m_mediaPlayer->position();
        
        // 确保位置总是有微小变化，强制解码器处理新帧
        if (position == lastPosition) {
            position += 1;
        }
        lastPosition = position;
        
        m_mp3Decoder->setPosition(position);
        
        // MP3解码器会通过回调更新频谱数据，这里不需要额外处理
    } else if (!isPlaying) {
        // 如果没有播放，逐渐降低所有频谱柱的高度
        QMutexLocker locker(&m_spectrumMutex);
        for (int i = 0; i < kBarsAmount; ++i) {
            // 缓慢降低频谱值
            m_spectrum[i] *= 0.95f;
            
            // 更新峰值位置
            if (m_spectrum[i] > m_peakPositions[i]) {
                m_peakPositions[i] = m_spectrum[i];
            } else {
                m_peakPositions[i] -= m_peakDecay;
                m_peakPositions[i] = qMax(m_peakPositions[i], m_spectrum[i]);
            }
        }
        
        m_spectrumDirty = true;
        update();
    }
}

/**
 * @brief 更新频谱显示
 */
void SpectrumBars::updateSpectrum()
{
    update();
    m_spectrumDirty = false;
}

void SpectrumBars::updateFrame()
{
    processAudio();      // 处理音频
    updateSpectrum();    // 触发重绘
    qDebug() << "Spectrumars::updateFrame 当前音频播放到了：" << m_mediaPlayer->position() << "毫秒";
}

/**
 * @brief 处理定时器事件，实现峰值指示器的平滑衰减动画效果
 */
void SpectrumBars::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timerId) {
        // 动画峰值衰减
        for (int i = 0; i < m_peakPositions.size(); ++i) {
            if (m_peakPositions[i] > m_spectrum[i]) {
                // 峰值高于当前频谱值时，应用衰减
                m_peakPositions[i] -= m_peakDecay;
                // 确保峰值不低于当前频谱值
                m_peakPositions[i] = qMax(m_peakPositions[i], m_spectrum[i]);
                m_spectrumDirty = true;  // 标记需要重绘
            }
        }
    }
    
    QWidget::timerEvent(event);
}

/**
 * @brief 绘制事件
 */
void SpectrumBars::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 用完全透明颜色清除背景，确保频谱柱能够无缝融入UI
    painter.fillRect(rect(), QColor(0, 0, 0, 0));
    
    // 计算柱状图尺寸
    int totalWidth = width();
    int totalHeight = height();
    
    // 确保数据数组与柱状图数量匹配
    if (m_spectrum.size() != kBarsAmount) {
        m_spectrum.resize(kBarsAmount);
        m_peakPositions.resize(kBarsAmount);
    }
    
    // 计算间距以使柱状图左对齐
    int totalBarWidth = kBarsAmount * (m_barWidth + m_barSpacing) - m_barSpacing;
    int startX = 0; // 左对齐
    
    // 绘制频谱柱状图
    for (int i = 0; i < kBarsAmount; ++i) {
        // 计算柱状图位置
        int x = startX + i * (m_barWidth + m_barSpacing);
        
        // 根据频谱数据计算柱状图高度，使用简单的平方根映射
        float amplitude = m_spectrum[i];
        amplitude = std::sqrt(amplitude); // 简单的平方根映射，增强低振幅信号可见性
        
        // 计算最终高度，使用适中的乘数避免频谱柱过高
        int barHeight = qMin(static_cast<int>(amplitude * totalHeight * 1.2), totalHeight - 2);
        barHeight = qMax(barHeight, 2);  // 确保最小可见性
        
        // 为柱状图创建简单的渐变色
        QLinearGradient gradient(0, totalHeight - barHeight, 0, totalHeight);
        gradient.setColorAt(0.0, m_topColor);
        gradient.setColorAt(0.5, m_midColor); 
        gradient.setColorAt(1.0, m_bottomColor);
        
        // 绘制柱状图
        QRect barRect(x, totalHeight - barHeight, m_barWidth, barHeight);
        painter.fillRect(barRect, gradient);
        
        //绘制峰值指示器
        float peakAmplitude = m_peakPositions[i];
        peakAmplitude = std::sqrt(peakAmplitude); // 与柱状图使用相同的映射
        int peakY = totalHeight - static_cast<int>(peakAmplitude * totalHeight * 1.2);
        peakY = qMax(peakY, 1);  // 确保峰值可见
        
        QRect peakRect(x, peakY, m_barWidth, 2);
        painter.fillRect(peakRect, m_peakColor);
    }
}

void SpectrumBars::calculateLogFrequencyMapping()
{
    const int halfN = FFT_SIZE / 2; // 512
    const float logMinFreq = std::log10(MIN_FREQ);     // log10(20)
    const float logMaxFreq = std::log10(MAX_FREQ);     // log10(20000)
    const float logRange = logMaxFreq - logMinFreq;

    m_logMapping.resize(kBarsAmount);

    for (int bar = 0; bar < kBarsAmount; ++bar) {
        // 计算该柱子对应的对数频率
        float logFreq = logMinFreq + (bar / static_cast<float>(kBarsAmount)) * logRange;
        float freq = std::pow(10.0f, logFreq);

        // 转换为 FFT bin 索引
        int bin = static_cast<int>(freq * FFT_SIZE / m_sampleRate);
        bin = qBound(1, bin, halfN); // 避免 bin=0 (DC 分量)

        m_logMapping[bar] = bin;
    }
}
