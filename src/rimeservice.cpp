/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */
#include "rimeservice.h"
#include "dbus_public.h"
#include "rimeengine.h"
#include "rimestate.h"

namespace fcitx {

RimeService::RimeService(RimeEngine *engine) : engine_(engine) {
    auto dbus = engine->dbus();
    if (!dbus) {
        return;
    }
    auto bus = dbus->call<IDBusModule::bus>();
    bus->addObjectVTable("/rime", "org.fcitx.Fcitx.Rime1", *this);
}

RimeState *RimeService::currentState() {
    auto ic = engine_->instance()->mostRecentInputContext();
    if (!ic) {
        return nullptr;
    }
    return engine_->state(ic);
}

void RimeService::setAsciiMode(bool ascii) {
    if (auto *state = currentState()) {
        state->setLatinMode(ascii);
        if (auto *ic = engine_->instance()->mostRecentInputContext();
            ic && ic->hasFocus()) {
            engine_->instance()->showInputMethodInformation(ic);
        }
    }
}

bool RimeService::isAsciiMode() {
    bool isAscii = false;
    if (auto *state = currentState()) {
        state->getStatus([&isAscii](const RimeStatus &status) {
            isAscii = status.is_ascii_mode;
        });
    }
    return isAscii;
}

void RimeService::setSchema(const std::string &schema) {
    if (auto state = currentState()) {
        state->selectSchema(schema);
        if (auto ic = engine_->instance()->mostRecentInputContext();
            ic && ic->hasFocus()) {
            engine_->instance()->showInputMethodInformation(ic);
        }
    }
}

std::string RimeService::currentSchema() {
    std::string result;
    auto state = currentState();
    if (state) {
        state->getStatus([&result](const RimeStatus &status) {
            result = status.schema_id ? status.schema_id : "";
        });
    }
    return result;
}

std::vector<std::string> RimeService::listAllSchemas() {
    std::vector<std::string> schemas;
    if (auto api = engine_->api()) {
        RimeSchemaList list;
        list.size = 0;
        if (api->get_schema_list(&list)) {
            for (size_t i = 0; i < list.size; i++) {
                schemas.emplace_back(list.list[i].schema_id);
            }
            api->free_schema_list(&list);
        }
    }
    return schemas;
}

} // namespace fcitx
