/*
 * SPDX-FileCopyrightText: 2020~2020 xzhao9 <i@xuzhao.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _RIME_GUI_MAIN_H
#define _RIME_GUI_MAIN_H

#include <fcitxqtconfiguiplugin.h>

namespace fcitx::rime {

class RimeConfigPlugin : public FcitxQtConfigUIPlugin {
    Q_OBJECT
public:
    Q_PLUGIN_METADATA(IID FcitxQtConfigUIFactoryInterface_iid FILE
                      "rime-config.json")
    explicit RimeConfigPlugin(QObject *parent = nullptr);
    FcitxQtConfigUIWidget *create(const QString &key) override;
};

} // namespace fcitx::rime

#endif // _RIME_GUI_MAIN_H
