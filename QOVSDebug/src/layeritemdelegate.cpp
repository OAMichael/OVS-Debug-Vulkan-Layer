#include "layeritemdelegate.h"

#include <QPainter>
#include <QProxyStyle>

static constexpr int IconTextSpacing = 10;
static constexpr int TextHOffset = 20;
static constexpr int FileNameFontHeight = 14;

class LayerItemStyle : public QProxyStyle
{
public:
    QRect subElementRect(SubElement subElement, const QStyleOption *option, const QWidget *widget) const override
    {
        QRect rect = QProxyStyle::subElementRect(subElement, option, widget);
        if (subElement == QStyle::SE_ItemViewItemText) {
            rect.setLeft(rect.left() + IconTextSpacing);
            rect.setHeight(rect.height() - TextHOffset);
        }
        return rect;
    }
};

LayerItemDelegate::LayerItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
    , style(new LayerItemStyle)
{
}

LayerItemDelegate::~LayerItemDelegate()
{
    delete style;
}

void LayerItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    QStyledItemDelegate::initStyleOption(&opt, index);

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    int hspacing = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, opt.widget) + 1;
    int vspacing = style->pixelMetric(QStyle::PM_FocusFrameVMargin, &opt, opt.widget) + 1;

    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);

    QString fileName = index.data(Qt::UserRole).toString();
    QRect fileNameRect = opt.rect;
    fileNameRect.setLeft(textRect.left() + hspacing);
    fileNameRect.setTop(textRect.top() + textRect.height() - 2 * vspacing);

    opt.font.setPixelSize(FileNameFontHeight);
    opt.font.setItalic(true);
    opt.font.setBold(true);

    painter->save();

    painter->setPen(opt.palette.color(QPalette::Highlight));
    painter->drawRect(opt.rect);

    painter->setFont(opt.font);
    painter->setPen(opt.palette.color(QPalette::Text));
    painter->drawText(fileNameRect, Qt::AlignLeft | Qt::AlignTop, fileName);

    painter->restore();
}

QSize LayerItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setWidth(size.width() + IconTextSpacing);
    return size;
}
