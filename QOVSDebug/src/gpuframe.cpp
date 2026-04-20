#include <gpuframe.h>
#include <gpuzone.h>

#include <QStyleOptionGraphicsItem>
#include <QPainter>

QGPUFrame::QGPUFrame(float begin, float duration, float height, uint32_t frame, const QColor &color)
{
    this->begin = begin;
    this->duration = duration;
    this->height = height;
    this->frame = frame;
    this->color = color;
    setZValue(0.5);

    setAcceptHoverEvents(true);

    desc = QString("Frame %1").arg(frame);

    QString tooltip = desc + " - ";
    if (duration > 1000000.0f) {
        tooltip += QString("%1 s").arg(duration / 1000000.0f);
    }
    else if (duration > 1000.0f) {
        tooltip += QString("%1 ms").arg(duration / 1000.0f);
    }
    else {
        tooltip += QString("%1 us").arg(duration);
    }

    setToolTip(tooltip);
}

QRectF QGPUFrame::boundingRect() const
{
    return QRectF(0, 0, duration, height);
}

void QGPUFrame::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    QColor fillColor = color;
    if (option->state & QStyle::State_MouseOver) {
        fillColor.setAlphaF(fillColor.alphaF() * 0.6f);
        setCursor(Qt::PointingHandCursor);
    }

    QPen pen(Qt::black, 0.0f);
    QBrush brush(fillColor);
    QRectF rect = boundingRect();

    qreal s = scale() / painter->transform().m11();
    qreal lod = option->levelOfDetailFromTransform(painter->worldTransform());
    rect.adjust(s, 1, -s, -1);

    painter->save();
    painter->setPen(pen);
    painter->setBrush(brush);
    painter->drawRect(rect);
    if (lod >= 1.0 && rect.width() > 0) {
        QPen textPen(Qt::darkGray, 0.0f);
        painter->setPen(textPen);

        QFont font = painter->font();
        font.setPixelSize(QGPUZone::getHeightValue() * 0.66);
        font.setItalic(true);
        painter->setFont(font);

        rect.setWidth(rect.width() / s);
        painter->scale(s, 1.0f);
        painter->drawText(rect, Qt::AlignHCenter | Qt::AlignBottom, desc);
    }
    painter->restore();
}
