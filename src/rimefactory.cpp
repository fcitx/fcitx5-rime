/*
 * SPDX-FileCopyrightText: 2024-2024 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "rimefactory.h"
#include "rimeengine.h"
#include <fcitx-utils/i18n.h>

namespace fcitx::rime {

AddonInstance *RimeEngineFactory::create(AddonManager *manager) {
    registerDomain("fcitx5-rime", FCITX_INSTALL_LOCALEDIR);
    return new RimeEngine(manager->instance());
}

} // namespace fcitx::rime

FCITX_ADDON_FACTORY_V2(rime, fcitx::rime::RimeEngineFactory)
