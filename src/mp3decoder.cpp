/*
 * @author :HPC2H2
 * @date   :2025-08-12
 * @version:1.2
 * @brief  :MP3解码器类的实现，用于解码MP3文件并提供实时音频数据
 *          (Implementation of MP3Decoder class for decoding MP3 files 
 *          and providing real-time audio data)
 */
#include <QFile>
#include <QDebug>
#include <QDateTime>
#include <algorithm>
#include <cmath>

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#define MINIMP3_EX_IMPLEMENTATION
#include "minimp3_ex.h"

#include "mp3decoder.h"

extern "C" {
#include "kiss_fft.h"
}

/**
 * @brief 构造函数
 */
MP3Decoder::MP3Decoder(QObject* parent)
    : QThread(parent),
      m_currentPosition(0),
      m_sampleRate(0),
      m_channels(0),
      m_fftCfg(nullptr),
      m_fftIn(FFT_SIZE),
      m_fftOut(FFT_SIZE)
{
    // 初始化 FFT 配置
    m_fftCfg = kiss_fft_alloc(FFT_SIZE, 0, nullptr, nullptr);
    if (!m_fftCfg) {
        qWarning() << "MP3Decoder：KissFFT初始化失败！";
    }
}

/**
 * @brief 设置频谱数据更新回调
 */
void MP3Decoder::setSpectrumCallback(SpectrumCallback callback)
{
    m_spectrumCallback = callback;
}

/**
 * @brief 析构函数
 * 
 * 停止解码线程并等待其结束，确保资源被正确释放。
 */
MP3Decoder::~MP3Decoder()
{
    stopDecoding();
    if (!wait(1000)) {
        qWarning() << "MP3Decoder线程未能正常结束，强制终止";
        terminate();
        wait();
    }

    // 释放 FFT 资源
    if (m_fftCfg) {
        free(m_fftCfg);
        m_fftCfg = nullptr;
    }
}

/**
 * @brief 打开MP3文件以获取解码信息，返回是否成功打开的布尔值
 *
 */
bool MP3Decoder::openFile(const QString& filePath)
{
    QMutexLocker locker(&m_mutex);

    m_filePath = filePath;
    m_currentPosition = 0;
    m_decodedPosition = 0;

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开MP3文件:" << m_filePath;
        return false;
    }

    m_fileData = file.readAll();
    file.close();

    if (m_fileData.isEmpty()) {
        qWarning() << "MP3文件为空:" << m_filePath;
        return false;
    }

    // 初始化解码器（不关闭，留给 run() 使用）
    memset(&m_mp3d, 0, sizeof(m_mp3d));
    if (mp3dec_ex_open_buf(&m_mp3d, (const uint8_t*)m_fileData.constData(), m_fileData.size(), MP3D_SEEK_TO_SAMPLE) != 0) {
        qWarning() << "MP3解码失败:" << m_filePath;
        return false;
    }

    m_sampleRate = m_mp3d.info.hz;
    m_channels = m_mp3d.info.channels;

    qDebug() << "成功打开MP3文件:" << m_filePath
             << "采样率:" << m_sampleRate
             << "通道数:" << m_channels;

    if (!isRunning()) {
        start();
    }

    return true;
}

/**
 * @brief 设置当前播放位置
 * @param position 播放位置（毫秒）
 *
 * 用于同步解码器的播放位置与媒体播放器的位置。
 * 解码线程会根据这个位置来解码相应的音频帧。
 */
void MP3Decoder::setPosition(qint64 position)
{
    QMutexLocker locker(&m_mutex);
    m_currentPosition = position;
}

/**
 * @brief 获取解码后的音频数据
 * @param numSamples 请求的样本数
 * @return 音频样本数据
 * 
 * 返回指定数量的音频样本，用于音频播放。
 * 如果没有足够的数据，返回空数组。
 * 返回数据后，会从缓冲区中移除这些数据。
 */
std::vector<float> MP3Decoder::getAudioData(int numSamples)
{
    QMutexLocker locker(&m_mutex);
    
    // 如果没有足够的数据，返回空数组
    if (m_audioData.size() < numSamples) {
        return std::vector<float>();
    }
    
    // 返回请求的样本数
    std::vector<float> result(m_audioData.begin(), m_audioData.begin() + numSamples);
    
    // 移除已使用的数据，避免重复使用
    m_audioData.erase(m_audioData.begin(), m_audioData.begin() + numSamples);
    
    return result;
}

/**
 * @brief 获取频谱数据
 * @return 频谱数据
 * 
 * 返回当前的频谱数据，用于频谱可视化。
 * 频谱数据是音频数据的频域表示，表示不同频率的能量分布。
 */
std::vector<float> MP3Decoder::getSpectrumData()
{
    QMutexLocker locker(&m_mutex);
    return m_spectrumData;
}

/**
 * @brief 停止解码
 * 
 * 设置线程停止标志，使解码线程安全退出。
 * 解码线程会在检查到这个标志后结束运行。
 * 同时请求中断线程，确保线程能够及时响应停止请求。
 */
void MP3Decoder::stopDecoding()
{
    requestInterruption();
}

/**
 * @brief 使用 FFT 计算音频信号的频谱（防饱和优化版）
 * @param samples 时域样本数据（浮点，归一化到 [-1,1]）
 */
void MP3Decoder::computeSpectrum(const std::vector<float>& samples)
{
    if (samples.empty() || !m_fftCfg) return;

    const int n = FFT_SIZE;
    const int halfN = n / 2;

    // 1. 加窗 FFT
    for (int i = 0; i < n; ++i) {
        float sample = (i < samples.size()) ? samples[i] : 0.0f;
        float window = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (n - 1)));
        m_fftIn[i].r = sample * window;
        m_fftIn[i].i = 0.0f;
    }

    kiss_fft(m_fftCfg, m_fftIn.data(), m_fftOut.data());

    // 2. 转 dBFS
    std::vector<float> normalized(halfN);
    const float kMinDb = -40.0f;  // 最小显示阈值
    const float kMaxDb = -3.0f;   // 参考最大值（留 3dB headroom）

    for (int i = 0; i < halfN; ++i) {
        float re = m_fftOut[i].r;
        float im = m_fftOut[i].i;
        float mag = std::sqrt(re*re + im*im);
        float db = 20.0f * std::log10(mag + 1e-9f);

        // 固定范围映射
        float clampedDb = std::clamp(db, kMinDb, kMaxDb);
        normalized[i] = (clampedDb - kMinDb) / (kMaxDb - kMinDb);

        // qDebug() << "频点" << i
        //          << "| 频率 ≈" << (i * 44100.0f / FFT_SIZE) << "Hz"
        //          << "| 原始响度 =" << db << "dBFS"
        //          << "| 显示响度 =" << clampedDb << "dB"
        //          << "| 输出值 =" << normalized[i]  // 这是传给 UI 的 [0,1] 值
        //          << "(将用于设置柱子高度)";

        // // gamma 校正，提升暗部
        // normalized[i] = std::pow(normalized[i], 0.75f);
    }

    // 3. 线程安全更新
    {
        QMutexLocker locker(&m_mutex);
        m_spectrumData = normalized;
    }

    if (m_spectrumCallback) {
        m_spectrumCallback(normalized);
    }
}

/**
 * @brief 线程执行函数
 * 
 * 在单独的线程中执行MP3解码，生成音频数据和频谱数据。
 * 根据当前播放位置解码相应的音频帧，并通过回调函数通知观察者。
 */
void MP3Decoder::run()
{
    // 确保解码器已初始化
    if (m_sampleRate == 0) {
        qWarning() << "解码器未初始化，采样率为0";
        return;
    }

    const size_t maxBufferSize = static_cast<size_t>(m_sampleRate) * 2; // 2秒缓冲
    const int numBands = 41;
    mp3d_sample_t buffer[MINIMP3_MAX_SAMPLES_PER_FRAME];

    // 用于跟踪上次 seek 的位置
    bool m_isFirstFrame = true;
    qint64 lastSeekPosition = -1;
    qint64 lastProcessTime = 0;

    while (!isInterruptionRequested()) {
        // 1. 获取目标位置
        qint64 targetPos;
        {
            QMutexLocker locker(&m_mutex);
            targetPos = m_currentPosition;
        }

        // 2. 判断是否需要跳转（首次或跳转超过100ms）
        bool shouldSeek = m_isFirstFrame || std::abs(targetPos - lastSeekPosition) > 100;

        // 确保即使位置没有变化，也至少每50ms处理一次新帧
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        bool shouldProcess = (currentTime - lastProcessTime) > 50;

        if (shouldSeek) {
            size_t samplePos = static_cast<size_t>(targetPos * static_cast<double>(m_sampleRate) / 1000.0);
            if (mp3dec_ex_seek(&m_mp3d, samplePos) == 0) {
                lastSeekPosition = targetPos;
                m_isFirstFrame = false;
                lastProcessTime = currentTime;
            } else {
                qWarning() << "MP3跳转失败:" << targetPos << "ms";
            }
        } else if (shouldProcess) {
            lastProcessTime = currentTime;
        } else {
            msleep(1);
            continue;
        }

        // 3. 解码一帧
        size_t samplesRead = mp3dec_ex_read(&m_mp3d, buffer, MINIMP3_MAX_SAMPLES_PER_FRAME);

        if (samplesRead == 0) {
            // 文件结束，可选择循环或退出
            msleep(10);
            continue;
        } else if (static_cast<qint64>(samplesRead) < 0) {
            qWarning() << "MP3解码错误";
            break;
        }

        // 4. 转换为浮点样本
        std::vector<float> samples;
        samples.reserve(samplesRead);
        for (size_t i = 0; i < samplesRead; ++i) {
            samples.push_back(buffer[i] / 32768.0f);
        }

        // 5. 计算帧时长（毫秒）
        qint64 frameDurationMs = static_cast<qint64>(samplesRead / m_channels / m_sampleRate * 1000.0);

        // 6. 更新音频缓冲
        {
            QMutexLocker locker(&m_mutex);
            m_audioData.insert(m_audioData.end(), samples.begin(), samples.end());
            if (m_audioData.size() > maxBufferSize) {
                size_t removeCount = m_audioData.size() - maxBufferSize;
                m_audioData.erase(m_audioData.begin(), m_audioData.begin() + removeCount);
            }
        }

        // 7. 使用 FFT 计算真实频谱（自动更新 m_spectrumData 并触发回调）
        computeSpectrum(samples);

        // 8. 动态休眠，避免CPU占用过高
        if (m_audioData.size() < maxBufferSize / 2) {
            msleep(1);
        } else {
            // 缓冲充足，不休眠，快速解码
        }
    }

    // 清理资源
    mp3dec_ex_close(&m_mp3d);
    memset(&m_mp3d, 0, sizeof(m_mp3d));
}
