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
#ifdef QT_MULTIMEDIA_ENABLED
#include <QAudioBuffer>
#include <QAudioOutput>
#endif
#include <QRandomGenerator>
#include <QtMath>
#include <QDebug>
#include <QDateTime>
#include <cmath>


/**
 * @brief 构造函数
 */
SpectrumBars::SpectrumBars(QWidget *parent)
    : QWidget(parent),
#ifdef QT_MULTIMEDIA_ENABLED
      m_mediaPlayer(nullptr),
#endif
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
    m_smoothedSpectrum.resize(kBarsAmount, 0.0f);

    // 初始化 auto-scale 历史缓冲区（原始幅度值域，100 是典型音乐的中等值）
    m_maxHistory.resize(kScaleHistorySize, 100.0f);
    
    // 设置峰值衰减动画（更快响应）
    m_peakAnimation->setDuration(150);
    m_peakAnimation->setStartValue(0.05);    // 中等衰减
    m_peakAnimation->setEndValue(0.4);       // 较快衰减
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
#ifdef QT_MULTIMEDIA_ENABLED
void SpectrumBars::setMediaPlayer(QMediaPlayer *player)
{
    m_mediaPlayer = player;

    if (m_mediaPlayer) {
        // 在媒体源变化时触发解码（去重：避免与 mediaStatusChanged 重复调用）
        connect(m_mediaPlayer, &QMediaPlayer::sourceChanged, this, [this](const QUrl &source) {
            if (source.isLocalFile()) {
                m_currentFilePath = source.toLocalFile();
            } else {
                m_currentFilePath.clear();
            }

            // 源变化时必须停掉旧解码线程，否则：
            // ① 旧线程读到文件末尾后空转(isRunning=true)，阻塞后续 openFile（防重入）
            // ② 自动循环播放时旧线程不再产出频谱数据 → 频谱"卡顿"
            if (m_mp3Decoder && m_mp3Decoder->isRunning()) {
                qDebug() << "SpectrumBars: sourceChanged，停旧解码器以准备重开";
                m_mp3Decoder->stopDecoding();
                m_mp3Decoder->wait();
            }
        });

        // 媒体加载完成后才启动解码（统一入口，避免重复 openFile）
        connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            if ((status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia)
                && !m_currentFilePath.isEmpty()) {
                tryGetRealAudioData();
            }
        });

        // 监听播放状态变化，用于同步频谱显示状态
        connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, this, &SpectrumBars::handlePlaybackStateChanged);

        connect(m_updateTimer, &QTimer::timeout, this, &SpectrumBars::updateFrame);
        // 注意：不在此处启动定时器，等 PlayingState 时再启动
    }
}
#endif // QT_MULTIMEDIA_ENABLED

/**
 * @brief 处理播放状态变化
 * @param state 新的播放状态
 * 
 * 当播放器状态变化时调用，用于同步频谱显示状态。
 */
#ifdef QT_MULTIMEDIA_ENABLED
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
            {
                QMutexLocker locker(&m_spectrumMutex);
                for (int i = 0; i < kBarsAmount; ++i) {
                    m_spectrum[i] *= 0.3f;
                    m_smoothedSpectrum[i] *= 0.3f;
                    m_peakPositions[i] = m_smoothedSpectrum[i];
                }
            }
            // 在锁外调用 update()
            update();
        }

        // 停止动画定时器
        if (m_timerId) {
            killTimer(m_timerId);
            m_timerId = 0;
        }
    }
}
#endif // QT_MULTIMEDIA_ENABLED

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

    // 防重入：如果解码器正在运行同一文件，跳过
    if (m_mp3Decoder->isRunning()) {
        qDebug() << "SpectrumBars: 解码器正在运行，跳过重复 openFile";
        return;
    }

    QFile file(m_currentFilePath);
    if (!file.exists()) {
        qWarning() << "音频文件不存在:" << m_currentFilePath;
        return;
    }

    if (m_mp3Decoder->openFile(m_currentFilePath)) {
        // 回调运行在解码器子线程中，只更新数据，不调用 GUI 操作（如 update）
        // 完整流水线：频带映射 → bass衰减 → dB门限映射 → 帧间平滑
        // 完整流水线（严格遵循 AudioSpectrum 的算法）
        // Step 1: FFT归一化 + 对数频带映射
        // Step 2: A计权感知加权（替代 bass 衰减，更科学）
        // Step 3: 增益放大（AudioSpectrum 用 *5）
        // Step 4: 空间平滑（卷积核 [1,2,3,5,3,2,1]）
        // Step 5: 时间平滑（EMA）
        // ================================================================
        m_mp3Decoder->setSpectrumCallback([this](const std::vector<float>& rawSpectrum) {
            QMutexLocker locker(&m_spectrumMutex);

            if (m_spectrum.size() != kBarsAmount) {
                m_spectrum.resize(kBarsAmount, 0.0f);
                m_peakPositions.resize(kBarsAmount, 0.0f);
                m_smoothedSpectrum.resize(kBarsAmount, 0.0f);
            }

            const int fftSize = FFT_SIZE;  // 归一化因子
            constexpr float kGain = 20.0f;  // 增益放大（原 AudioSpectrum 用 *5，此处需更大以补偿 A计权衰减和 EMA 压缩）

            // --- Step 1 & 2: Log-freq band mapping + FFT归一化 + A计权 ---
            for (int bar = 0; bar < kBarsAmount; ++bar) {
                int startBin = (bar == 0) ? 2 : m_logMapping[bar - 1];
                int endBin = m_logMapping[bar];

                float maxMag = 0.0f;
                for (int bin = startBin; bin < endBin && bin < rawSpectrum.size(); ++bin) {
                    maxMag = qMax(maxMag, rawSpectrum[bin]);
                }

                // FFT 归一化（AudioSpectrum: fftNormFactor = 1/fftSize）
                float normalized = maxMag / static_cast<float>(fftSize);

                // --- Step 2: A计权感知加权 ---
                // 计算该 bar 的中心频率
                float centerFreq = 0.0f;
                for (int bin = startBin; bin < endBin && bin < rawSpectrum.size(); ++bin) {
                    centerFreq += bin * (static_cast<float>(m_sampleRate) / fftSize);
                }
                int binCount = qMax(1, endBin - startBin);
                centerFreq /= binCount;

                // A计权近似公式（简化版，对 20Hz~20kHz 有效）
                // 参考 IEC 61672-1 标准的 A-weighting 曲线
                float f2 = centerFreq * centerFreq;
                float ra = 12194.0f * 12194.0f;
                float aWeight = 1.2589f * ra * f2 * f2 /
                    ((f2 + 20.6f) * sqrt((f2 + 107.7f) * (f2 + 737.9f)) *
                     (f2 + ra));
                aWeight = std::max(aWeight, 0.001f);  // 防止除零或负值

                normalized *= aWeight;

                // --- Step 3: 增益放大 ---
                m_spectrum[bar] = normalized * kGain;
            }

            // --- Step 4: 空间平滑（AudioSpectrum 的 highlightWaveform 卷积）---
            // 卷积核: [1, 2, 3, 5, 3, 2, 1] (权重和=17)
            // 效果：锐化波峰、填充凹陷，使频谱更连贯饱满
            static constexpr int kernelSize = 7;
            static constexpr float kernel[kernelSize] = {1.0f, 2.0f, 3.0f, 5.0f, 3.0f, 2.0f, 1.0f};
            static constexpr float kernelSum = 17.0f;

            std::vector<float> smoothed(kBarsAmount);
            for (int bar = 0; bar < kBarsAmount; ++bar) {
                float convVal = 0.0f;
                for (int ki = 0; ki < kernelSize; ++ki) {
                    int neighborIdx = bar + ki - kernelSize / 2;
                    if (neighborIdx >= 0 && neighborIdx < kBarsAmount) {
                        convVal += m_spectrum[neighborIdx] * kernel[ki];
                    } else {
                        convVal += m_spectrum[bar] * kernel[ki];  // 边界外复制自身
                    }
                }
                smoothed[bar] = convVal / kernelSum;
            }
            m_spectrum = std::move(smoothed);

            // --- Step 5: 时间平滑（EMA，AudioSpectrum 的 spectrumSmooth=0.5）---
            for (int bar = 0; bar < kBarsAmount; ++bar) {
                // AudioSpectrum 公式: newValue = oldValue * smooth + current * (1-smooth)
                const float smooth = 0.55f;  // 比 AudioSpectrum 的 0.5 稍粘滞一点
                m_smoothedSpectrum[bar] =
                    m_smoothedSpectrum[bar] * smooth + m_spectrum[bar] * (1.0f - smooth);

                // 峰值指示器
                if (m_smoothedSpectrum[bar] > m_peakPositions[bar]) {
                    m_peakPositions[bar] = m_smoothedSpectrum[bar];
                } else {
                    m_peakPositions[bar] -= m_peakDecay;
                    m_peakPositions[bar] = qMax(m_peakPositions[bar], m_smoothedSpectrum[bar]);
                }
            }

            // 诊断日志
            static int diagCounter = 0;
            if (++diagCounter >= 300) {
                diagCounter = 0;
                qDebug() << "[SPEC-DBG]"
                         << " bar[0]=" << m_smoothedSpectrum[0]
                         << " bar[10]=" << m_smoothedSpectrum[10]
                         << " bar[20]=" << m_smoothedSpectrum[20]
                         << " bar[40]=" << m_smoothedSpectrum[40];
            }

            m_spectrumDirty = true;
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
    if (!m_mp3Decoder) {
        return;
    }

#ifdef QT_MULTIMEDIA_ENABLED
    if (!m_mediaPlayer) {
        return;
    }
#endif

    // 更新解码器位置
    m_mp3Decoder->setPosition(position);

    // 如果播放器处于播放状态，确保更新定时器正在运行
#ifdef QT_MULTIMEDIA_ENABLED
    if (m_mediaPlayer && m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
#else
    if (true) {  // 无 Multimedia 时始终允许更新
#endif
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
#ifdef QT_MULTIMEDIA_ENABLED
    bool isPlaying = m_mediaPlayer && m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState;
#else
    bool isPlaying = false;
#endif

    // 如果正在播放且MP3解码器已实例化，更新解码器的位置
    if (isPlaying && m_mp3Decoder) {
#ifdef QT_MULTIMEDIA_ENABLED
        static qint64 lastPosition = 0;
        qint64 position = m_mediaPlayer->position();

        // 确保位置总是有微小变化，强制解码器处理新帧
        if (position == lastPosition) {
            position += 1;
        }
        lastPosition = position;

        m_mp3Decoder->setPosition(position);

        // MP3解码器会通过回调更新频谱数据，这里不需要额外处理
#endif
    } else if (!isPlaying) {
        // 如果没有播放，逐渐降低所有频谱柱的高度
        {
            QMutexLocker locker(&m_spectrumMutex);
            for (int i = 0; i < kBarsAmount; ++i) {
                // 缓慢降低频谱值（同时衰减平滑值）
                m_spectrum[i] *= 0.92f;
                m_smoothedSpectrum[i] *= 0.92f;

                // 更新峰值位置
                if (m_smoothedSpectrum[i] > m_peakPositions[i]) {
                    m_peakPositions[i] = m_smoothedSpectrum[i];
                } else {
                    m_peakPositions[i] -= m_peakDecay;
                    m_peakPositions[i] = qMax(m_peakPositions[i], m_smoothedSpectrum[i]);
                }
            }

            m_spectrumDirty = true;
        }
        // 在锁外调用 update()，避免潜在的 paintEvent 重入风险
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
#ifdef QT_MULTIMEDIA_ENABLED
    // 仅在正在播放且每秒输出一次（避免刷屏）
    static qint64 lastLogTime = 0;
    if (m_mediaPlayer && m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - lastLogTime >= 1000) {
            lastLogTime = now;
            qDebug() << "[主线程] updateFrame 播放位置:" << m_mediaPlayer->position() << "ms"
                     << "时间:" << now;
        }
    }
#endif
}

/**
 * @brief 处理定时器事件，实现峰值指示器的平滑衰减动画效果
 */
void SpectrumBars::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timerId) {
        // 动画峰值衰减（加锁保护跨线程安全）
        QMutexLocker locker(&m_spectrumMutex);
        for (int i = 0; i < m_peakPositions.size(); ++i) {
            if (m_peakPositions[i] > m_spectrum[i]) {
                m_peakPositions[i] -= m_peakDecay;
                m_peakPositions[i] = qMax(m_peakPositions[i], m_spectrum[i]);
                m_spectrumDirty = true;
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

    // 快照频谱数据（加锁保护跨线程安全）
    std::vector<float> spectrumSnap;
    std::vector<float> peakSnap;
    {
        QMutexLocker locker(&m_spectrumMutex);
        if (m_spectrum.size() != kBarsAmount) {
            m_spectrum.resize(kBarsAmount);
            m_peakPositions.resize(kBarsAmount);
            m_smoothedSpectrum.resize(kBarsAmount);
        }
        spectrumSnap = m_smoothedSpectrum;  // 使用平滑后的数据绘制
        peakSnap = m_peakPositions;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 用完全透明颜色清除背景
    painter.fillRect(rect(), QColor(0, 0, 0, 0));

    const int totalHeight = height();
    const int totalWidth = width();

    // 绘制频谱柱状图（AudioSpectrum 方式：amplitude 直接线性映射到像素）
    // m_smoothedSpectrum 的值域：典型音乐 ≈ 0.05 ~ 1.5，峰值可达 2.0+
    // 总体设计：正常音量时大部分 bar 在 10%~60% 高度，强节拍时部分 bar 可达 80%~100%
    for (int i = 0; i < kBarsAmount; ++i) {
        int x = i * (m_barWidth + m_barSpacing);

        float amplitude = spectrumSnap[i];
        if (amplitude < 0.005f) {
            // 极低能量：画最小可见点（避免空隙）
            int h = qMax(1, static_cast<int>(totalHeight / 30));
            painter.fillRect(x, totalHeight - h, m_barWidth, h, m_bottomColor);
            continue;
        }

        // 线性映射到像素（AudioSpectrum 方式：直接乘以可用高度，加放大系数）
        constexpr float kVisualScale = 2.5f;  // 视觉放大，补偿 A计权和 EMA 压缩后的值域收缩
        int barHeight = static_cast<int>(amplitude * kVisualScale * (totalHeight - 4));
        barHeight = qMax(barHeight, 1);       // 最少 1px
        barHeight = qMin(barHeight, totalHeight - 2);  // 不超出控件

        // 渐变色（从顶部亮色到底部深色）
        QLinearGradient gradient(0, totalHeight - barHeight, 0, totalHeight);
        gradient.setColorAt(0.0, m_topColor);
        gradient.setColorAt(0.6, m_midColor);
        gradient.setColorAt(1.0, m_bottomColor);

        painter.fillRect(x, totalHeight - barHeight, m_barWidth, barHeight, gradient);

        // 峰值指示器（同样用线性映射 + 放大系数）
        float peakVal = peakSnap[i];
        if (peakVal < 0.01f) continue;
        int peakH = static_cast<int>(peakVal * kVisualScale * (totalHeight - 4));
        peakH = qMax(peakH, 2);
        peakH = qMin(peakH, totalHeight - 2);
        int peakY = totalHeight - peakH;

        if (peakY < totalHeight - barHeight - 3) {
            painter.fillRect(x, peakY, m_barWidth, 2, m_peakColor);
        }
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
