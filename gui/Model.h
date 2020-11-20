/*
 * SPDX-FileCopyrightText: 2020~2020 xzhao9 <i@xuzhao.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _RIME_GUI_MODEL_H
#define _RIME_GUI_MODEL_H

#include "RimeConfigParser.h"
#include <QList>
#include <fcitx-utils/key.h>
#include <rime_api.h>

static constexpr int defaultPageSize = 5;

namespace fcitx::rime {

struct FcitxRimeSchema {
    QString path;
    QString id;
    QString name;
    int index; // index starts from 1, 0 means not enabled
    bool active;
};

class RimeConfigDataModel {
public:
    int candidatePerWord_;
    QList<SwitchKeyFunction> switchKeys_;
    QList<FcitxRimeSchema> schemas_;
    QList<Key> toggleKeys_;
    QList<Key> asciiKeys_;
    QList<Key> trasimKeys_;
    QList<Key> halffullKeys_;
    QList<Key> pgupKeys_;
    QList<Key> pgdownKeys_;

    void setKeybindings(const std::vector<KeyBinding> bindings);
    std::vector<KeyBinding> getKeybindings();

    void sortSchemas();
    void sortKeys();

private:
    void sortSingleKeySet(QList<Key> &keys);
};

} // namespace fcitx::rime

#endif // _RIME_GUI_MODEL_H
