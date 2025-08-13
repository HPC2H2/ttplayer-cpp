/*
 * @author :HPC2H2
 * @date   :2025-08-12
 * @version:1.2
 * @brief  :MP3解码器类，用于解码MP3文件并提供实时音频数据
 *          (MP3 decoder class for decoding MP3 files and providing real-time audio data)
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

// 添加 KissFFT 头文件
extern "C" {
#include "kiss_fft.h"
}

/**
 * @class MP3Decoder
 * @brief MP3解码器类，用于解码MP3文件并提供实时音频数据
 * 
 * MP3Decoder类继承自QThread，在单独的线程中解码MP3文件，
 * 提供实时音频数据和频谱数据，用于音频播放和频谱可视化。
 * 该类使用minimp3库进行MP3解码。
 */
class MP3Decoder : public QThread
{
public:
    /**
     * @brief 频谱数据更新回调函数类型
     */
    // 使用typedef减少代码量
    typedef std::function<void(const std::vector<float>&)> SpectrumCallback;
    
    MP3Decoder(QObject* parent = nullptr);

    ~MP3Decoder();
    
    /**
     * @brief 打开MP3文件
     */
    bool openFile(const QString& filePath);
    
    /**
     * @brief 设置当前播放位置
     * @param position 播放位置（毫秒）
     * 
     * 用于同步解码器的播放位置与媒体播放器的位置。
     */
    void setPosition(qint64 position);
    
    /**
     * @brief 获取解码后的音频数据
     * @param numSamples 请求的样本数
     * @return 音频样本数据
     * 
     * 返回指定数量的音频样本，用于音频播放。
     * 如果没有足够的数据，返回空数组。
     */
    std::vector<float> getAudioData(int numSamples);
    
    /**
     * @brief 获取频谱数据
     * @return 频谱数据
     * 
     * 返回当前的频谱数据，用于频谱可视化。
     */
    std::vector<float> getSpectrumData();
    
    /**
     * @brief 停止解码
     * 
     * 设置线程停止标志，使解码线程安全退出。
     */
    void stopDecoding();
    
    /**
     * @brief 设置频谱数据更新回调
     * @param callback 回调函数
     * 
     * 设置在频谱数据更新时调用的回调函数。
     */
    void setSpectrumCallback(SpectrumCallback callback);
    

    /**
     * @brief 获取音频采样率
     * @return 采样率（Hz）
     */
    int sampleRate() {  // 去掉 const
        QMutexLocker locker(&m_mutex);
        return m_sampleRate;
    }

    /**
     * @brief 获取音频通道数
     * @return 通道数（1=单声道，2=立体声）
     */
    int channels() {
        QMutexLocker locker(&m_mutex);
        return m_channels;
    }

protected:
    /**
     * @brief 线程执行函数
     * 
     * 在单独的线程中执行MP3解码，生成音频数据和频谱数据。
     * 根据当前播放位置解码相应的音频帧，并通过回调函数通知观察者。
     * 
     * 注意：此方法由QThread内部调用，不应直接调用。
     * 使用start()方法启动线程，使用wait()等待线程结束。
     */
    void run() override;
    
private:
    // FFT 相关成员和函数
    static constexpr int FFT_SIZE = 1024;        // FFT 窗口大小（2的幂）
    static constexpr int SPECTRUM_BINS = 41;     // 频谱柱数量

    kiss_fft_cfg m_fftCfg;                       // FFT 配置句柄
    std::vector<kiss_fft_cpx> m_fftIn;           // FFT 输入缓冲区
    std::vector<kiss_fft_cpx> m_fftOut;          // FFT 输出缓冲区
    void computeSpectrum(const std::vector<float>& timeDomainSamples);
    
    // 解码相关成员和函数
    QString m_filePath;               // MP3文件路径
    qint64 m_currentPosition;         // 当前播放位置（毫秒）
    QMutex m_mutex;                   // 互斥锁，保护共享数据
    std::vector<float> m_audioData;   // 解码后的音频数据缓冲区
    std::vector<float> m_spectrumData;// 频谱数据
    int m_sampleRate;                 // 采样率（Hz）
    int m_channels;                   // 通道数（1=单声道，2=立体声）
    SpectrumCallback m_spectrumCallback; // 频谱数据更新回调函数

    QByteArray m_fileData;            // 文件数据（避免生命周期问题）
    mp3dec_ex_t m_mp3d;               // 解码器状态（提升为成员变量）
    qint64 m_decodedPosition;         // 已解码到的时间位置（毫秒）
};

#endif // MP3DECODER_H
