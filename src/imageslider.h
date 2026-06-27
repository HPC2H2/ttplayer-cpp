#ifndef IMAGESLIDER_H
#define IMAGESLIDER_H

#include <QSlider>
#include <QPixmap>
#include <QPaintEvent>

class ImageSlider : public QSlider
{
    Q_OBJECT

public:
    explicit ImageSlider(const QPixmap &pixmap, QWidget *parent = nullptr);

    int currentVolume() const { return m_currentVolume; }
    void setCurrentVolume(int volume) { m_currentVolume = volume; }

    // 动态更新滑块手柄图片（用于换肤时更新）
    void setThumbImage(const QPixmap &pixmap) { m_handlePixmap = pixmap; update(); }
    void setPosition(qint64 pos) { setValue(static_cast<int>(pos)); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_handlePixmap;
    int m_currentVolume;
};

#endif // IMAGESLIDER_H