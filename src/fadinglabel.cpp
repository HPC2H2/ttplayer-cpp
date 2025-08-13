#include "fadinglabel.h"
#include <QFont>

FadingLabel::FadingLabel(const QString &text, QWidget *parent)
    : QLabel(text, parent),
      m_effect(new QGraphicsOpacityEffect(this)),
      m_animation(new QPropertyAnimation(m_effect, "opacity"))
{
    // Center align text
    setAlignment(Qt::AlignCenter);
    
    // Set font style
    QFont font;
    font.setPointSize(14);  // Smaller font size to fit more text
    font.setBold(true);
    setFont(font);
    
    // Enable word wrap to handle long text
    setWordWrap(true);
    
    // Setup opacity effect and animation
    setGraphicsEffect(m_effect);
    
    m_animation->setDuration(800);  // Animation duration in ms
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);
}

FadingLabel::~FadingLabel()
{
    // Disconnect any connections to avoid crashes during destruction
    if (m_animation) {
        m_animation->stop();
        disconnect(m_animation, nullptr, this, nullptr);
    }
    
    // Note: m_effect and m_animation are automatically deleted 
    // because they have this as their parent
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

// Override setText to handle text that might exceed the display area
void FadingLabel::setText(const QString &text)
{
    // Ensure text fits within the label by enabling word wrapping
    setWordWrap(true);
    
    // Set text with elision if it's too long
    QString elidedText = fontMetrics().elidedText(text, Qt::ElideRight, width() - 10);
    QLabel::setText(elidedText);
}
