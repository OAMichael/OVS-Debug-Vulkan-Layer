#include <gpuzone.h>

#include <QGraphicsSceneMouseEvent>
#include <QStyleOptionGraphicsItem>
#include <QPainter>

QGPUZone::QGPUZone(const QString &name, float begin, float duration, const QColor &color)
{
    this->name = name;
    this->begin = begin;
    this->duration = duration;
    this->color = color;
    setZValue(0.0);

    setFlags(ItemIsSelectable);
    setAcceptHoverEvents(true);

    QString tooltip = QString("%1: %2 us").arg(name).arg(duration);
    setToolTip(tooltip);
}

QRectF QGPUZone::boundingRect() const
{
    return QRectF(0, 0, duration, getHeightValue());
}

void QGPUZone::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    QColor fillColor = color;
    if (option->state & QStyle::State_MouseOver) {
        fillColor = fillColor.lighter(150);
        setCursor(Qt::PointingHandCursor);
    }
    else if (option->state & QStyle::State_Selected) {
        fillColor = fillColor.darker(150);
    }

    QPen pen(fillColor.darker(300), 0.0f);
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
        QFont font = painter->font();
        font.setPixelSize(getHeightValue() * 2 / 5);
        painter->setFont(font);

        rect.setWidth(rect.width() / s);
        painter->scale(s, 1.0f);
        painter->drawText(rect, Qt::AlignCenter, name);
    }
    painter->restore();
}

void QGPUZone::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mousePressEvent(event);
    update();
}

void QGPUZone::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
    update();
}
