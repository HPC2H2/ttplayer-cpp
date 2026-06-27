/*
 * Minimal FFT Implementation (替代 KissFFT)
 *
 * 自包含的快速傅里叶变换实现，仅包含 ttplayer 频谱分析所需的最低功能。
 * 无外部依赖，可直接嵌入项目。
 *
 * 支持:
 *   - 复数正向/反向 FFT (Cooley-Tukey radix-2 算法)
 *   - 实数输入的优化版本 (rfft)
 *
 * 使用方式:
 *   FFT fft(1024);           // 创建 N 点 FFT 对象
 *   fft.forward(in, out);    // 正向 FFT (复数->复数)
 *   fft.rforward(in, out);   // 实数正向 FFT (实数->复数，输出 N/2+1 点)
 */
#ifndef TTPLAYER_FFT_H
#define TTPLAYER_FTT_H

#include <vector>
#include <complex>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct FftComplex {
    float r = 0.0f; // 实部
    float i = 0.0f; // 虚部

    FftComplex() = default;
    FftComplex(float re, float im) : r(re), i(im) {}
    FftComplex operator+(const FftComplex& o) const { return {r + o.r, i + o.i}; }
    FftComplex operator-(const FftComplex& o) const { return {r - o.r, i - o.i}; }
    FftComplex operator*(const FftComplex& o) const {
        return {r*o.r - i*o.i, r*o.i + i*o.r};
    }
    void operator+=(const FftComplex& o) { r += o.r; i += o.i; }
    void operator*=(const FftComplex& o) {
        float nr = r*o.r - i*o.i;
        i = r*o.i + i*o.r;
        r = nr;
    }
};

class FFT
{
public:
    // 构造指定大小的 FFT（size 必须是 2 的幂）
    explicit FFT(int size);

    // 正向复数 FFT: in[0..N-1] -> out[0..N-1]
    void forward(const std::vector<FftComplex>& in, std::vector<FftComplex>& out);

    // 反向复数 FFT (未缩放): in[0..N-1] -> out[0..N-1]
    void inverse(const std::vector<FftComplex>& in, std::vector<FftComplex>& out);

    // 实数正向 FFT: in[0..N-1] (float) -> out[0..N/2] (FftComplex)
    // 输出大小为 size/2 + 1 (正频率部分)
    void rforward(const std::vector<float>& in, std::vector<FftComplex>& out);

    int size() const { return m_size; }

private:
    int m_size;
    int m_logSize;
    // 预计算的旋转因子 (twiddle factors)
    std::vector<FftComplex> m_twiddle;

    void computeTwiddle();
    void bitReverse(std::vector<FftComplex>& data);
};

// ========== 内联实现 ==========

inline FFT::FFT(int size)
    : m_size(size)
{
    // 验证是 2 的幂
    if (size <= 0 || (size & (size - 1)) != 0) {
        m_size = 1024; // 默认回退
    }

    m_logSize = 0;
    int tmp = m_size;
    while (tmp > 1) { m_logSize++; tmp >>= 1; }

    computeTwiddle();
}

inline void FFT::computeTwiddle()
{
    m_twiddle.resize(m_size / 2);
    for (int k = 0; k < m_size / 2; ++k) {
        float angle = static_cast<float>(-2.0 * M_PI * k / m_size);
        m_twiddle[k] = FftComplex(cosf(angle), sinf(angle));
    }
}

inline void FFT::bitReverse(std::vector<FftComplex>& data)
{
    int j = 0;
    for (int i = 0; i < m_size - 1; ++i) {
        if (i < j) {
            FftComplex tmp = data[i];
            data[i] = data[j];
            data[j] = tmp;
        }
        int k = m_size >> 1;
        while (k <= j) { j -= k; k >>= 1; }
        j += k;
    }
}

inline void FFT::forward(const std::vector<FftComplex>& in, std::vector<FftComplex>& out)
{
    out = in;
    bitReverse(out);

    // Cooley-Tukey butterfly
    for (int stage = 1; stage <= m_logSize; ++stage) {
        int m = 1 << stage;     // 当前阶段的 FFT 大小
        int halfM = m >> 1;
        for (int k = 0; k < m_size; k += m) {
            for (int j = 0; j < halfM; ++j) {
                FftComplex t = out[k + j + halfM] * m_twiddle[j * (m_size / m)];
                out[k + j + halfM] = out[k + j] - t;
                out[k + j] = out[k + j] + t;
            }
        }
    }
}

inline void FFT::inverse(const std::vector<FftComplex>& in, std::vector<FftComplex>& out)
{
    out = in;
    bitReverse(out);

    for (int stage = 1; stage <= m_logSize; ++stage) {
        int m = 1 << stage;
        int halfM = m >> 1;
        for (int k = 0; k < m_size; k += m) {
            for (int j = 0; j < halfM; ++j) {
                // 反向 FFT 使用共轭旋转因子
                FftComplex twiddle(m_twiddle[j * (m_size / m)].r,
                                   -m_twiddle[j * (m_size / m)].i);
                FftComplex t = out[k + j + halfM] * twiddle;
                out[k + j + halfM] = out[k + j] - t;
                out[k + j] = out[k + j] + t;
            }
        }
    }

    // 缩放 1/N
    float scale = 1.0f / m_size;
    for (auto& c : out) {
        c.r *= scale;
        c.i *= scale;
    }
}

inline void FFT::rforward(const std::vector<float>& in, std::vector<FftComplex>& out)
{
    // 打包实数输入为复数数组
    std::vector<FftComplex> data(m_size);
    for (int i = 0; i < m_size && i < (int)in.size(); ++i) {
        data[i] = FftComplex(in[i], 0.0f);
    }

    forward(data, data);

    // 提取前 N/2+1 个频点（利用对称性，实数输入的 FFT 结果共轭对称）
    int halfN = m_size / 2 + 1;
    out.resize(halfN);
    for (int i = 0; i < halfN; ++i) {
        out[i] = data[i];
    }
}

#endif // TTPLAYER_FTT_H
