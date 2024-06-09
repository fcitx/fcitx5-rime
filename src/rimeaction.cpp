/*
 * SPDX-FileCopyrightText: 2024~2024 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "rimeaction.h"
#include "rimeengine.h"
#include <algorithm>
#include <cstddef>
#include <fcitx-utils/stringutils.h>
#include <fcitx/action.h>
#include <fcitx/inputcontext.h>
#include <fcitx/userinterfacemanager.h>
#include <optional>
#include <rime_api.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fcitx {

namespace {

std::optional<bool> optionValue(RimeEngine *engine, InputContext *ic,
                                bool requestSession,
                                const std::string &option) {
    auto *state = engine->state(ic);
    auto *api = engine->api();
    if (!state) {
        return std::nullopt;
    }
    auto session = state->session(requestSession);
    if (!session) {
        return std::nullopt;
    }
    return bool(api->get_option(session, option.c_str()));
}
} // namespace

ToggleAction::ToggleAction(RimeEngine *engine, std::string_view schema,
                           std::string_view option, std::string disabledText,
                           std::string enabledText)
    : engine_(engine), option_(option), disabledText_(std::move(disabledText)),
      enabledText_(std::move(enabledText)) {
    engine_->instance()->userInterfaceManager().registerAction(
        stringutils::concat("fcitx-rime-", schema, "-", option), this);
}

void ToggleAction::activate(InputContext *ic) {
    auto *state = engine_->state(ic);
    auto *api = engine_->api();
    if (!state) {
        return;
    }
    // Do not send notification since user is explicitly select it.
    engine_->blockNotificationFor(30000);
    auto session = state->session();
    Bool oldValue = api->get_option(session, option_.c_str());
    api->set_option(session, option_.c_str(), !oldValue);
}

std::string ToggleAction::shortText(InputContext *ic) const {
    auto value = optionValue(engine_, ic, /*requestSession=*/true, option_);
    if (!value.has_value()) {
        return "";
    }
    if (*value) {
        return stringutils::concat(enabledText_, " → ", disabledText_);
    }
    return stringutils::concat(disabledText_, " → ", enabledText_);
}

std::optional<std::string> ToggleAction::snapshotOption(InputContext *ic) {
    auto value = optionValue(engine_, ic, /*requestSession=*/false, option_);
    if (!value.has_value()) {
        return std::nullopt;
    }
    return *value ? option_ : stringutils::concat("!", option_);
}

bool ToggleAction::checkOptionName(std::string_view name) const {
    return name == option_;
}

std::string ToggleAction::optionLabel(InputContext *ic) {
    auto value = optionValue(engine_, ic, /*requestSession=*/true, option_);
    if (!value.has_value()) {
        return "";
    }
    return *value ? enabledText_ : disabledText_;
}

SelectAction::SelectAction(RimeEngine *engine, std::string_view schema,
                           std::vector<std::string> options,
                           std::vector<std::string> texts)
    : engine_(engine), options_(options), texts_(std::move(texts)) {
    for (size_t i = 0; i < options.size(); ++i) {
        actions_.emplace_back();
        actions_.back().setShortText(texts_[i]);
        actions_.back().connect<SimpleAction::Activated>(
            [this, i](InputContext *ic) {
                auto *state = engine_->state(ic);
                auto *api = engine_->api();
                if (!state) {
                    return;
                }
                auto session = state->session();
                for (size_t j = 0; j < options_.size(); ++j) {
                    api->set_option(session, options_[j].c_str(), i == j);
                }
            });
        engine_->instance()->userInterfaceManager().registerAction(
            stringutils::concat("fcitx-rime-", schema, "-", options_[i]),
            &actions_.back());
        menu_.addAction(&actions_.back());
    }
    setMenu(&menu_);
    engine_->instance()->userInterfaceManager().registerAction(
        stringutils::concat("fcitx-rime-", schema, "-select-", options[0]),
        this);
}

std::string SelectAction::shortText(InputContext *ic) const {
    auto *state = engine_->state(ic);
    auto *api = engine_->api();
    if (!state) {
        return "";
    }
    auto session = state->session();
    for (size_t i = 0; i < options_.size(); ++i) {
        if (api->get_option(session, options_[i].c_str())) {
            return texts_[i];
        }
    }
    return "";
}

std::optional<std::string> SelectAction::snapshotOption(InputContext *ic) {
    auto *state = engine_->state(ic);
    auto *api = engine_->api();
    if (!state) {
        return std::nullopt;
    }
    auto session = state->session(false);
    if (!session) {
        return std::nullopt;
    }
    for (auto &option : options_) {
        if (api->get_option(session, option.c_str())) {
            return option;
        }
    }
    return std::nullopt;
}

bool SelectAction::checkOptionName(std::string_view name) const {
    return std::find(options_.begin(), options_.end(), name) != options_.end();
}

std::string SelectAction::optionLabel(InputContext *ic) {
    return shortText(ic);
}
} // namespace fcitx
