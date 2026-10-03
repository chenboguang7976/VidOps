#include "ui/ProgressDelegate.h"

#include <QApplication>
#include <QPainter>
#include <QStyleOptionProgressBar>

namespace vidops {

void ProgressDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    QStyleOptionViewItem bg = option;
    initStyleOption(&bg, index);
    bg.text.clear();
    QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &bg, painter, option.widget);

    const int value = index.data(Qt::UserRole).toInt();
    QStyleOptionProgressBar bar;
    bar.initFrom(option.widget);
    bar.rect = option.rect.adjusted(3, 3, -3, -3);
    bar.minimum = 0;
    bar.maximum = 100;
    bar.progress = qMax(0, value);
    bar.text = index.data(Qt::DisplayRole).toString();
    bar.textVisible = true;
    bar.textAlignment = Qt::AlignCenter;
    bar.state = QStyle::State_Enabled | QStyle::State_Horizontal;
    style->drawControl(QStyle::CE_ProgressBar, &bar, painter, option.widget);
}

}  // namespace vidops
