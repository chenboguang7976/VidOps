#pragma once

#include <QStyledItemDelegate>

namespace vidops {

// Draws a progress bar from Qt::UserRole (0..100, -1 = unknown).
class ProgressDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
};

}  // namespace vidops
