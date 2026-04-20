#ifndef GPUFRAME_H
#define GPUFRAME_H

#include <QColor>
#include <QString>
#include <QGraphicsItem>

class QGPUFrame: public QGraphicsItem
{
public:
    QGPUFrame(float begin, float duration, float height, uint32_t frame, const QColor &color);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *item, QWidget *widget) override;

private:

    QString desc;
    float begin{0.0f};
    float duration{0.0f};
    float height{0.0f};
    uint32_t frame{0};
    QColor color;
};

#endif // GPUFRAME_H
