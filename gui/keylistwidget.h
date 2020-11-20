/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */
#ifndef _KCM_FCITX_KEYLISTWIDGET_H_
#define _KCM_FCITX_KEYLISTWIDGET_H_

#include <QWidget>
#include <fcitx-utils/key.h>

class QToolButton;
class QBoxLayout;

namespace fcitx {
namespace kcm {

class KeyListWidget : public QWidget {
    Q_OBJECT
public:
    explicit KeyListWidget(QWidget *parent = 0);

    QList<Key> keys() const;
    void setKeys(const QList<Key> &keys);
    void setAllowModifierLess(bool);
    void setAllowModifierOnly(bool);

signals:
    void keyChanged();

protected:
    void resizeEvent(QResizeEvent *) override;

private:
    void addKey(Key key = Key());
    bool removeKeyAt(int idx);
    bool showRemoveButton() const;

    QToolButton *addButton_;
    QBoxLayout *keysLayout_;
    bool modifierLess_ = false;
    bool modifierOnly_ = false;
};

} // namespace kcm
} // namespace fcitx

#endif // _KCM_FCITX_KEYLISTWIDGET_H_
