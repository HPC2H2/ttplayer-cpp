/*
 * audioplayer.cpp - Windows 原生音频输出实现
 */
#include "audioplayer.h"
#include "mp3decoder.h"

#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

#include <QDebug>
#include <QTimer>
#include <QFile>
#include <algorithm>

// ============================================================
// 构造/析构
// ============================================================

AudioPlayer::AudioPlayer(QObject* parent)
    : QObject(parent)
{
}

AudioPlayer::~AudioPlayer()
{
    stop();
}

// ============================================================
// waveOut 回调（在系统线程中调用，必须快速返回）
// ============================================================

void CALLBACK AudioPlayer::waveOutProc(HWAVEOUT /*hwo*/, UINT uMsg,
    DWORD_PTR dwInstance, DWORD_PTR /*dwParam1*/, DWORD_PTR /*dwParam2*/)
{
    if (uMsg != WOM_DONE) return;

    // 通知主线程可以填充新数据
    AudioPlayer* self = reinterpret_cast<AudioPlayer*>(dwInstance);
    if (self && self->m_playing.load() && !self->m_paused.load()) {
        QMetaObject::invokeMethod(self, &AudioPlayer::feedBuffer, Qt::QueuedConnection);
    }
}

// ============================================================
// 初始化/清理 waveOut
// ============================================================

void AudioPlayer::initWaveOut(int sampleRate, int channels)
{
    cleanupWaveOut();

    m_sampleRate = sampleRate;
    m_channels = channels;

    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = static_cast<WORD>(channels);
    wfx.nSamplesPerSec = static_cast<DWORD>(sampleRate);
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    MMRESULT result = waveOutOpen(&m_hWaveOut, WAVE_MAPPER, &wfx,
        (DWORD_PTR)waveOutProc, (DWORD_PTR)this,
        CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        qCritical() << "[AudioPlayer] waveOutOpen 失败:" << result;
        m_hWaveOut = nullptr;
        return;
    }

    qDebug() << "[AudioPlayer] waveOut 初始化成功:"
             << sampleRate << "Hz" << channels << "ch";

    // 预分配缓冲区
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        void* buf = malloc(BUFFER_BYTES);
        memset(buf, 0, BUFFER_BYTES);

        WAVEHDR hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.lpData = (LPSTR)buf;
        hdr.dwBufferLength = BUFFER_BYTES;
        hdr.dwFlags = 0;

        result = waveOutPrepareHeader(m_hWaveOut, &hdr, sizeof(WAVEHDR));
        if (result == MMSYSERR_NOERROR) {
            m_waveBuffers.append(buf);
            m_waveHeaders.append(hdr);
        } else {
            free(buf);
            qWarning() << "[AudioPlayer] waveOutPrepareHeader 失败:" << result;
        }
    }
}

void AudioPlayer::cleanupWaveOut()
{
    if (!m_hWaveOut) return;

    waveOutReset(m_hWaveOut);

    for (int i = 0; i < m_waveHeaders.size(); ++i) {
        waveOutUnprepareHeader(m_hWaveOut, &m_waveHeaders[i], sizeof(WAVEHDR));
    }
    for (int i = 0; i < m_waveBuffers.size(); ++i) {
        free(m_waveBuffers[i]);
    }
    m_waveHeaders.clear();
    m_waveBuffers.clear();

    waveOutClose(m_hWaveOut);
    m_hWaveOut = nullptr;
}

// ============================================================
// 缓冲区喂入
// ============================================================

void AudioPlayer::feedBuffer()
{
    if (!m_hWaveOut || !m_playing.load() || m_paused.load()) return;

    // 找一个空闲缓冲区
    for (int i = 0; i < m_waveHeaders.size(); ++i) {
        WAVEHDR& hdr = m_waveHeaders[i];
        if (hdr.dwFlags & WHDR_INQUEUE) continue;  // 还在播放中

        // 从队列取 PCM 数据并转换
        QMutexLocker locker(&m_queueMutex);

        int samplesNeeded = BUFFER_SAMPLES;
        short* outBuf = reinterpret_cast<short*>(hdr.lpData);

        int filled = 0;
        while (filled < samplesNeeded && !m_pcmQueue.empty()) {
            float sample = m_pcmQueue.front();
            m_pcmQueue.erase(m_pcmQueue.begin());

            // 音量 + 16bit 转换
            float volFactor = m_volume / 100.0f;
            int val = static_cast<int>(sample * 32767.0f * volFactor);
            val = std::max(-32768, std::min(32767, val));

            // 填充所有声道
            for (int ch = 0; ch < m_channels; ++ch) {
                outBuf[filled++] = static_cast<short>(val);
                if (filled >= samplesNeeded) break;
            }
        }

        // 不足部分填零
        while (filled < samplesNeeded) {
            outBuf[filled++] = 0;
        }

        locker.unlock();

        hdr.dwBufferLength = BUFFER_BYTES;
        hdr.dwFlags = 0;

        MMRESULT result = waveOutWrite(m_hWaveOut, &hdr, sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR) {
            qWarning() << "[AudioPlayer] waveOutWrite 失败:" << result;
        }
        break;  // 每次只填一个缓冲
    }
}

// ============================================================
// 公开接口
// ============================================================

bool AudioPlayer::playFile(const QString& filePath)
{
    qDebug() << "[AudioPlayer] playFile() called with:" << filePath;

    stop();

    m_decoder = new MP3Decoder(this);
    connect(m_decoder, &QThread::finished, this, &AudioPlayer::onDecoderFinished);

    qDebug() << "[AudioPlayer] 打开文件...";
    if (!m_decoder->openFile(filePath)) {
        delete m_decoder;
        m_decoder = nullptr;
        qCritical() << "[AudioPlayer] 无法打开文件:" << filePath;
        return false;
    }

    qDebug() << "[AudioPlayer] 文件打开成功, sampleRate=" << m_decoder->sampleRate()
             << "channels=" << m_decoder->channels();

    m_sampleRate = m_decoder->sampleRate();
    m_channels = m_decoder->channels();

    // 估算时长：读取文件大小来估算
    QFile f(filePath);
    qint64 fileSize = f.size();  // bytes
    // 时长(ms) ≈ 文件大小(bytes) * 8 / 码率(bps) * 1000
    // 对于 CBR MP3: duration = size * 8 / (128000) * 1000 = size * 0.0625
    m_duration = static_cast<qint64>(fileSize * 62.5);  // 近似 ms
    emit durationChanged(m_duration);

    initWaveOut(m_sampleRate, m_channels);
    if (!m_hWaveOut) {
        delete m_decoder;
        m_decoder = nullptr;
        return false;
    }

    m_paused = false;
    m_playing = true;
    m_position = 0;

    emit playbackStateChanged(true);

    // 启动位置更新定时器
    QTimer* posTimer = new QTimer(this);
    connect(posTimer, &QTimer::timeout, this, [this, posTimer]() {
        if (m_playing && !m_paused) {
            m_position.store(m_position.load() + 50);  // 每 50ms 更新一次
            if (m_position > m_duration) m_position = m_duration;
            emit positionChanged(m_position.load());

            // 定期从解码器取数据喂入缓冲
            feedBuffer();
        }
        if (!m_playing) {
            posTimer->stop();
            posTimer->deleteLater();
        }
    });
    posTimer->start(50);

    // 启动 PCM 数据采集定时器
    QTimer* pcmTimer = new QTimer(this);
    connect(pcmTimer, &QTimer::timeout, this, [this]() {
        if (m_decoder && m_playing.load() && !m_paused.load()) {
            auto data = m_decoder->getAudioData(2048);
            if (!data.empty()) {
                QMutexLocker lock(&m_queueMutex);
                m_pcmQueue.insert(m_pcmQueue.end(), data.begin(), data.end());
            }
        }
    });
    pcmTimer->start(10);  // 10ms 采集一次

    // 先预填缓冲区
    feedBuffer();

    qDebug() << "[AudioPlayer] 开始播放:" << filePath
             << "- 预估时长:" << m_duration << "ms";
    return true;
}

void AudioPlayer::pause()
{
    if (!m_hWaveOut || !m_playing.load() || m_paused.load()) return;

    waveOutPause(m_hWaveOut);
    m_paused = true;
    emit playbackStateChanged(false);
    qDebug() << "[AudioPlayer] 已暂停";
}

void AudioPlayer::resume()
{
    if (!m_hWaveOut || !m_playing.load() || !m_paused.load()) return;

    waveOutRestart(m_hWaveOut);
    m_paused = false;
    emit playbackStateChanged(true);
    qDebug() << "[AudioPlayer] 已恢复";
}

void AudioPlayer::stop()
{
    m_playing = false;
    m_paused = false;

    cleanupWaveOut();

    if (m_decoder) {
        m_decoder->stopDecoding();
        m_decoder->wait(2000);
        delete m_decoder;
        m_decoder = nullptr;
    }

    {
        QMutexLocker lock(&m_queueMutex);
        m_pcmQueue.clear();
    }

    m_position = 0;
    emit playbackStateChanged(false);
    emit positionChanged(0);
}

void AudioPlayer::setVolume(int volume)
{
    m_volume = qBound(0, volume, 100);
}

void AudioPlayer::setPosition(qint64 pos)
{
    m_position = pos;
    if (m_decoder) {
        m_decoder->setPosition(pos);
    }
    emit positionChanged(pos);
}

void AudioPlayer::onDecoderFinished()
{
    if (m_playing.load()) {
        m_playing = false;
        emit finished();
        emit playbackStateChanged(false);
    }
}
