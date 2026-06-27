/*
 * audioplayer.h - Windows 原生音频输出 (waveOut API)
 * 用于在没有 Qt Multimedia 的环境下播放解码后的 PCM 数据
 */
#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QVector>
#include <atomic>
#include <vector>

// Windows API 类型（仅用于声明）
#ifndef AUDIOPLAYER_NO_WIN
#include <windows.h>
#include <mmsystem.h>
#endif

class MP3Decoder;

/**
 * @brief 使用 Windows waveOut API 播放 PCM 音频数据的播放器
 *
 * 工作流程：
 * 1. 接收 MP3Decoder 解码出的 PCM 浮点采样
 * 2. 转换为 16bit PCM 格式
 * 3. 通过 waveOut 送入声卡缓冲区循环播放
 */
class AudioPlayer : public QObject
{
    Q_OBJECT

public:
    explicit AudioPlayer(QObject* parent = nullptr);
    ~AudioPlayer();

    /**
     * @brief 加载并开始播放 MP3 文件
     * @param filePath MP3 文件路径
     * @return 是否成功开始播放
     */
    bool playFile(const QString& filePath);

    /** 暂停播放 */
    void pause();

    /** 恢复播放 */
    void resume();

    /** 停止播放并释放资源 */
    void stop();

    /** 设置音量 (0-100) */
    void setVolume(int volume);

    /** 获取当前播放位置(毫秒) */
    qint64 position() const { return m_position; }

    /** 获取总时长(毫秒) */
    qint64 duration() const { return m_duration; }

    /** 设置播放位置(毫秒) */
    void setPosition(qint64 pos);

    /** 是否正在播放 */
    bool isPlaying() const { return m_playing; }

    /** 是否已暂停 */
    bool isPaused() const { return m_paused; }

signals:
    /** 位置变化 */
    void positionChanged(qint64 pos);
    /** 总时长变化 */
    void durationChanged(qint64 dur);
    /** 播放状态变化 */
    void playbackStateChanged(bool playing);
    /** 播放结束（文件播完） */
    void finished();

private slots:
    void onDecoderFinished();

private:
    void initWaveOut(int sampleRate, int channels);
    void cleanupWaveOut();
    void feedBuffer();

    // waveOut 回调函数
    static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg,
        DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);

    // 音频参数
    int m_sampleRate = 0;
    int m_channels = 0;
    int m_volume = 100;

    // waveOut 资源
    HWAVEOUT m_hWaveOut = nullptr;
    QVector<void*> m_waveBuffers;      // waveOut 缓冲区指针（用于 free）
    QVector<WAVEHDR> m_waveHeaders;    // waveOut 头部信息
    static constexpr int NUM_BUFFERS = 4;
    static constexpr int BUFFER_SAMPLES = 4096;  // 每个缓冲区的采样数
    static constexpr int BUFFER_BYTES = BUFFER_SAMPLES * sizeof(short);  // 16bit

    // 状态
    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_paused{false};
    std::atomic<qint64> m_position{0};
    qint64 m_duration = 0;

    // 解码器
    MP3Decoder* m_decoder = nullptr;

    // PCM 数据队列（从解码线程接收）
    std::vector<float> m_pcmQueue;
    QMutex m_queueMutex;
};

#endif // AUDIOPLAYER_H
