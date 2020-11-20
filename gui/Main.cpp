/*
 * SPDX-FileCopyrightText: 2020~2020 xzhao9 <i@xuzhao.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "Main.h"
#include "ConfigMain.h"
#include <fcitxqtconfiguiplugin.h>
#include <qplugin.h>

namespace fcitx::rime {

// FcitxQtConfigUIPlugin : QObject, FcitxQtConfigUIFactoryInterface
RimeConfigPlugin::RimeConfigPlugin(QObject *parent)
    : FcitxQtConfigUIPlugin(parent) {}

FcitxQtConfigUIWidget *RimeConfigPlugin::create(const QString &key) {
    Q_UNUSED(key);
    return new ConfigMain;
}

} // namespace fcitx::rime
