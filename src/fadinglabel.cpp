#include "fadinglabel.h"
#include <QFont>

FadingLabel::FadingLabel(const QString &text, QWidget *parent)
    : QLabel(text, parent)
{
    // Center align text
    setAlignment(Qt::AlignCenter);
    
    // Set font style
    QFont font;
    font.setPointSize(18);
    font.setBold(true);
    setFont(font);
    
    // Setup opacity effect and animation
    m_effect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(m_effect);
    
    m_animation = new QPropertyAnimation(m_effect, "opacity");
    m_animation->setDuration(800);  // Animation duration in ms
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);
}

void FadingLabel::fadeIn()
{
    m_animation->stop();
    m_effect->setOpacity(0);
    setVisible(true);
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->start();
}

void FadingLabel::fadeOut()
{
    m_animation->stop();
    m_effect->setOpacity(1.0);
    m_animation->setStartValue(1.0);
    m_animation->setEndValue(0.0);
    
    // Connect animation finished signal to hide the label
    connect(m_animation, &QPropertyAnimation::finished, this, [this]() {
        setVisible(false);
        // Disconnect to avoid multiple connections
        disconnect(m_animation, &QPropertyAnimation::finished, this, nullptr);
    });
    
    m_animation->start();
}