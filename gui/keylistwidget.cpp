/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "keylistwidget.h"
#include <QHBoxLayout>
#include <QToolButton>
#include <QVBoxLayout>
#include <fcitx-utils/i18n.h>
#include <fcitxqtkeysequencewidget.h>

namespace fcitx {
namespace kcm {

KeyListWidget::KeyListWidget(QWidget *parent) : QWidget(parent) {
    auto layout = new QHBoxLayout;
    layout->setMargin(0);
    keysLayout_ = new QVBoxLayout;
    keysLayout_->setMargin(0);
    auto subLayout = new QVBoxLayout;

    addButton_ = new QToolButton;
    addButton_->setAutoRaise(true);
    addButton_->setIcon(QIcon::fromTheme("list-add-symbolic"));
    addButton_->setText(_("Add"));
    addButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(addButton_, &QToolButton::clicked, this, [this]() {
        addKey(Key());
        emit keyChanged();
    });

    layout->addLayout(keysLayout_);
    subLayout->addWidget(addButton_, 0, Qt::AlignTop);
    // subLayout->addStretch(1);
    layout->addLayout(subLayout);

    setLayout(layout);

    // Add an empty one.
    addKey();
}

void KeyListWidget::addKey(fcitx::Key key) {
    auto keyWidget = new FcitxQtKeySequenceWidget;
    keyWidget->setClearButtonShown(false);
    keyWidget->setKeySequence({key});
    keyWidget->setModifierlessAllowed(modifierLess_);
    keyWidget->setModifierOnlyAllowed(modifierOnly_);
    auto widget = new QWidget;
    auto layout = new QHBoxLayout;
    layout->setMargin(0);
    layout->addWidget(keyWidget);
    auto removeButton = new QToolButton;
    removeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    removeButton->setIcon(QIcon::fromTheme("list-remove-symbolic"));
    removeButton->setText(_("Remove"));
    removeButton->setVisible(showRemoveButton());
    layout->addWidget(removeButton);
    widget->setLayout(layout);
    connect(removeButton, &QPushButton::clicked, widget, [widget, this]() {
        auto idx = keysLayout_->indexOf(widget);
        if (removeKeyAt(idx)) {
            emit keyChanged();
        }
    });
    connect(keyWidget, &FcitxQtKeySequenceWidget::keySequenceChanged, this,
            &KeyListWidget::keyChanged);
    connect(this, &KeyListWidget::keyChanged, removeButton,
            [this, removeButton]() {
                removeButton->setVisible(showRemoveButton());
            });
    keysLayout_->addWidget(widget);
}

void KeyListWidget::setKeys(const QList<fcitx::Key> &keys) {
    while (keysLayout_->count() > 1) {
        removeKeyAt(0);
    }
    removeKeyAt(0);

    bool first = true;
    for (auto key : keys) {
        if (first) {
            first = false;
            keysLayout_->itemAt(0)
                ->widget()
                ->findChild<FcitxQtKeySequenceWidget *>()
                ->setKeySequence({key});
        } else {
            addKey(key);
        }
    }
    emit keyChanged();
}

QList<fcitx::Key> KeyListWidget::keys() const {
    QList<fcitx::Key> result;
    for (int i = 0; i < keysLayout_->count(); i++) {
        if (auto keyWidget = keysLayout_->itemAt(i)
                                 ->widget()
                                 ->findChild<FcitxQtKeySequenceWidget *>()) {
            if (keyWidget->keySequence().isEmpty()) {
                continue;
            }
            auto &key = keyWidget->keySequence()[0];
            if (key.isValid() && !result.contains(key)) {
                result << keyWidget->keySequence()[0];
            }
        }
    }
    return result;
}

void KeyListWidget::setAllowModifierLess(bool value) {
    if (value == modifierLess_) {
        return;
    }

    modifierLess_ = value;

    for (int i = 0; i < keysLayout_->count(); i++) {
        if (auto keyWidget = keysLayout_->itemAt(i)
                                 ->widget()
                                 ->findChild<FcitxQtKeySequenceWidget *>()) {
            keyWidget->setModifierlessAllowed(modifierLess_);
        }
    }
}

void KeyListWidget::setAllowModifierOnly(bool value) {
    if (value == modifierOnly_) {
        return;
    }

    modifierOnly_ = value;

    for (int i = 0; i < keysLayout_->count(); i++) {
        if (auto keyWidget = keysLayout_->itemAt(i)
                                 ->widget()
                                 ->findChild<FcitxQtKeySequenceWidget *>()) {
            keyWidget->setModifierOnlyAllowed(modifierOnly_);
        }
    }
}

bool KeyListWidget::removeKeyAt(int idx) {
    if (idx < 0 || idx > keysLayout_->count()) {
        return false;
    }
    auto widget = keysLayout_->itemAt(idx)->widget();
    if (keysLayout_->count() == 1) {
        keysLayout_->itemAt(0)
            ->widget()
            ->findChild<FcitxQtKeySequenceWidget *>()
            ->setKeySequence(QList<Key>());
    } else {
        keysLayout_->removeWidget(widget);
        delete widget;
    }
    return true;
}

bool KeyListWidget::showRemoveButton() const {
    return keysLayout_->count() > 1 ||
           (keysLayout_->count() == 1 &&
            keysLayout_->itemAt(0)
                ->widget()
                ->findChild<FcitxQtKeySequenceWidget *>()
                ->keySequence()
                .size());
}

void KeyListWidget::resizeEvent(QResizeEvent *event) {
    if (keysLayout_->count() > 0) {
        addButton_->setMinimumHeight(
            keysLayout_->itemAt(0)
                ->widget()
                ->findChild<FcitxQtKeySequenceWidget *>()
                ->height());
        addButton_->setMaximumHeight(addButton_->minimumHeight());
    }

    QWidget::resizeEvent(event);
}

} // namespace kcm
} // namespace fcitx
