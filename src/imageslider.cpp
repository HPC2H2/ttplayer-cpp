#include "imageslider.h"
#include <QPainter>
#include <QPainterPath>

ImageSlider::ImageSlider(const QPixmap &pixmap, QWidget *parent)
    : QSlider(Qt::Horizontal, parent), m_handlePixmap(pixmap), m_currentVolume(60)
{
    setRange(0, 100);
    setValue(0);
}

void ImageSlider::paintEvent(QPaintEvent *event)
{
    // Don't call parent's paintEvent to avoid drawing the default slider
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Calculate slider position
    // Note: QSlider's groove width is the widget width minus the slider width
    int availableWidth = width() - m_handlePixmap.width();
    int minVal = minimum();
    int maxVal = maximum();
    int currentVal = value();
    
    // Map current value to x coordinate
    int handleX = static_cast<int>(availableWidth * (currentVal - minVal) / (maxVal - minVal));
    int handleY = (height() - m_handlePixmap.height()) / 2;  // Vertically centered
    
    // Draw the pixmap as the slider handle
    painter.drawPixmap(handleX, handleY, m_handlePixmap);
}