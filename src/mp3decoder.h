/*
 * @author :HPC2H2
 * @date   :2025-08-12
 * @version:2.0
 * @brief  :MP3解码器类，用于解码MP3文件并提供实时音频数据
 *          v2.0: 使用自有 FFT 实现，移除 KissFFT 依赖
 */
#ifndef MP3DECODER_H
#define MP3DECODER_H

#include <QThread>    // 线程支持
#include <QMutex>     // 互斥锁
#include <QString>    // 字符串处理
#include <vector>     // 标准向量容器
#include <functional> // 函数对象

#include "minimp3.h"     // 提供 mp3dec_t 等基础类型
#include "minimp3_ex.h"  // 提供 mp3dec_ex_t 声明
#include "fft.h"         // 自有 FFT 实现（替代 kissfft）

/**
 * @class MP3Decoder
 * @brief MP3解码器类，使用 minimp3 解码 + 自有 FFT 计算频谱
 *
 * 依赖说明:
 *   - minimp3/minimp3_ex: 仅用于 MP3 解码获取 PCM 数据（header-only, 内嵌）
 *   - fft.h: 自有实现，无外部依赖
 */
class MP3Decoder : public QThread
{
public:
    typedef std::function<void(const std::vector<float>&)> SpectrumCallback;

    MP3Decoder(QObject* parent = nullptr);
    ~MP3Decoder();

    bool openFile(const QString& filePath);
    void setPosition(qint64 position);
    std::vector<float> getAudioData(int numSamples);
    std::vector<float> getSpectrumData();
    void stopDecoding();
    void setSpectrumCallback(SpectrumCallback callback);

    int sampleRate() {
        QMutexLocker locker(&m_mutex);
        return m_sampleRate;
    }

    int channels() {
        QMutexLocker locker(&m_mutex);
        return m_channels;
    }

protected:
    void run() override;

private:
    static constexpr int FFT_SIZE = 1024;
    static constexpr int SPECTRUM_BINS = 41;

    // 使用自有 FFT 替代 kiss_fft_cfg
    std::unique_ptr<FFT> m_fft;                  // 自有 FFT 对象
    std::vector<FftComplex> m_fftIn;             // FFT 输入缓冲区
    void computeSpectrum(const std::vector<float>& timeDomainSamples);

    QString m_filePath;
    qint64 m_currentPosition = 0;
    QMutex m_mutex;
    std::vector<float> m_audioData;
    std::vector<float> m_spectrumData;
    int m_sampleRate = 0;
    int m_channels = 0;
    SpectrumCallback m_spectrumCallback;

    QByteArray m_fileData;
    mp3dec_ex_t m_mp3d;
    qint64 m_decodedPosition = 0;
};

#endif // MP3DECODER_H
