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

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_handlePixmap;
    int m_currentVolume;
};

#endif // IMAGESLIDER_H