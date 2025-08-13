#ifndef FADINGLABEL_H
#define FADINGLABEL_H

#include <QLabel>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>

class FadingLabel : public QLabel
{
    Q_OBJECT

public:
    explicit FadingLabel(const QString &text = "", QWidget *parent = nullptr);
    ~FadingLabel();
    
    // Fade in/out animations
    void fadeIn();
    void fadeOut();
    
    // Override setText to handle text that might exceed the display area
    void setText(const QString &text);

private:
    QGraphicsOpacityEffect *m_effect;
    QPropertyAnimation *m_animation;
};

#endif // FADINGLABEL_H