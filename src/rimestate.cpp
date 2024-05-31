/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "rimestate.h"
#include "rimecandidate.h"
#include "rimeengine.h"
#include <cstdint>
#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/stringutils.h>
#include <fcitx-utils/textformatflags.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/candidatelist.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/text.h>
#include <fcitx/userinterface.h>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <rime_api.h>
#include <string>
#include <utility>

namespace fcitx {

namespace {

bool emptyExceptAux(const InputPanel &inputPanel) {

    return inputPanel.preedit().empty() && inputPanel.preedit().empty() &&
           (!inputPanel.candidateList() || inputPanel.candidateList()->empty());
}
} // namespace

RimeState::RimeState(RimeEngine *engine, InputContext &ic)
    : engine_(engine), ic_(ic) {}

RimeState::~RimeState() {}

RimeSessionId RimeState::session(bool requestNewSession) {
    if (!session_ && requestNewSession) {
        auto [sessionHolder, isNewSession] =
            engine_->sessionPool().requestSession(&ic_);
        session_ = sessionHolder;
        if (isNewSession) {
            restore();
        } else {
            savedCurrentSchema_.clear();
            savedOptions_.clear();
        }
    }
    if (!session_) {
        return 0;
    }

    return session_->id();
}

void RimeState::clear() {
    if (auto session = this->session()) {
        engine_->api()->clear_composition(session);
    }
}

std::string RimeState::subMode() {
    std::string result;
    getStatus([&result](const RimeStatus &status) {
        if (status.is_disabled) {
            result = "\xe2\x8c\x9b";
        } else if (status.is_ascii_mode) {
            result = _("Latin Mode");
        } else if (status.schema_name && status.schema_name[0] != '.') {
            result = status.schema_name;
        }
    });
    return result;
}

std::string RimeState::subModeLabel() {
    std::string result;
    getStatus([&result](const RimeStatus &status) {
        if (status.is_disabled) {
            result = "";
        } else if (status.is_ascii_mode) {
            result = "A";
        } else if (status.schema_name && status.schema_name[0] != '.') {
            result = status.schema_name;
            if (!result.empty() &&
                utf8::lengthValidated(result) != utf8::INVALID_LENGTH) {
                result = result.substr(
                    0, std::distance(result.begin(),
                                     utf8::nextChar(result.begin())));
            }
        }
    });
    return result;
}

void RimeState::toggleLatinMode() {
    auto *api = engine_->api();
    if (api->is_maintenance_mode()) {
        return;
    }

    Bool oldValue = api->get_option(session(), RIME_ASCII_MODE);
    api->set_option(session(), RIME_ASCII_MODE, !oldValue);
}

void RimeState::setLatinMode(bool latin) {
    auto *api = engine_->api();
    if (api->is_maintenance_mode()) {
        return;
    }
    api->set_option(session(), RIME_ASCII_MODE, latin);
}

void RimeState::selectSchema(const std::string &schema) {
    auto *api = engine_->api();
    if (api->is_maintenance_mode()) {
        return;
    }
    engine_->blockNotificationFor(30000);
    api->set_option(session(), RIME_ASCII_MODE, false);
    api->select_schema(session(), schema.data());
}

void RimeState::keyEvent(KeyEvent &event) {
    auto *ic = event.inputContext();
    std::optional<std::string> compose;
    if (!event.key().states().testAny(
            KeyStates{KeyState::Ctrl, KeyState::Super}) &&
        !event.isRelease()) {
        compose =
            engine_->instance()->processComposeString(&ic_, event.key().sym());
        if (!compose) {
            event.filterAndAccept();
            return;
        }
    }

    auto *api = engine_->api();
    if (api->is_maintenance_mode()) {
        return;
    }
    auto session = this->session();
    if (!session) {
        return;
    }

    lastMode_ = subMode();
    auto states = event.rawKey().states() &
                  KeyStates{KeyState::Mod1, KeyState::CapsLock, KeyState::Shift,
                            KeyState::Ctrl, KeyState::Super};
    if (states.test(KeyState::Super)) {
        // IBus uses virtual super mask.
        states |= KeyState::Super2;
    }
    uint32_t intStates = states;
    if (event.isRelease()) {
        // IBUS_RELEASE_MASK
        intStates |= (1 << 30);
    }
    if (!compose->empty()) {
        event.filterAndAccept();
        auto length = utf8::lengthValidated(*compose);
        bool result = false;
        if (length == 1) {
            auto c = utf8::getChar(*compose);
            auto sym = Key::keySymFromUnicode(c);
            if (sym != FcitxKey_None) {
                result = api->process_key(session, sym, intStates);
            }
        }
        if (!result) {
            commitPreedit(ic);
            ic->commitString(*compose);
            clear();
        }
    } else {
        auto result =
            api->process_key(session, event.rawKey().sym(), intStates);
        if (result) {
            event.filterAndAccept();
        }
    }

    RIME_STRUCT(RimeCommit, commit);
    if (api->get_commit(session, &commit)) {
        ic->commitString(commit.text);
        api->free_commit(&commit);
        engine_->instance()->resetCompose(ic);
    }

    updateUI(ic, event.isRelease());
}

#ifndef FCITX_RIME_NO_SELECT_CANDIDATE
void RimeState::selectCandidate(InputContext *inputContext, int idx,
                                bool global) {
    auto *api = engine_->api();
    if (api->is_maintenance_mode()) {
        return;
    }
    auto session = this->session();
    if (!session) {
        return;
    }
    if (global) {
        api->select_candidate(session, idx);
    } else {
        api->select_candidate_on_current_page(session, idx);
    }
    RIME_STRUCT(RimeCommit, commit);
    if (api->get_commit(session, &commit)) {
        inputContext->commitString(commit.text);
        api->free_commit(&commit);
    }
    updateUI(inputContext, false);
}
#endif

bool RimeState::getStatus(
    const std::function<void(const RimeStatus &)> &callback) {
    auto *api = engine_->api();
    auto session = this->session();
    if (!session) {
        return false;
    }
    RIME_STRUCT(RimeStatus, status);
    if (!api->get_status(session, &status)) {
        return false;
    }
    callback(status);
    api->free_status(&status);
    return true;
}

Text preeditFromRimeContext(const RimeContext &context, TextFormatFlags flag,
                            TextFormatFlags highlightFlag) {
    Text preedit;

    do {
        if (context.composition.length == 0) {
            break;
        }

        // validation.
        if (!(context.composition.sel_start >= 0 &&
              context.composition.sel_start <= context.composition.sel_end &&
              context.composition.sel_end <= context.composition.length)) {
            break;
        }

        /* converted text */
        if (context.composition.sel_start > 0) {
            preedit.append(std::string(context.composition.preedit,
                                       context.composition.sel_start),
                           flag);
        }

        /* converting candidate */
        if (context.composition.sel_start < context.composition.sel_end) {
            preedit.append(
                std::string(
                    &context.composition.preedit[context.composition.sel_start],
                    &context.composition.preedit[context.composition.sel_end]),
                flag | highlightFlag);
        }

        /* remaining input to convert */
        if (context.composition.sel_end < context.composition.length) {
            preedit.append(
                std::string(
                    &context.composition.preedit[context.composition.sel_end],
                    &context.composition.preedit[context.composition.length]),
                flag);
        }

        preedit.setCursor(context.composition.cursor_pos);
    } while (0);

    return preedit;
}

void RimeState::updatePreedit(InputContext *ic, const RimeContext &context) {
    PreeditMode mode = ic->capabilityFlags().test(CapabilityFlag::Preedit)
                           ? *engine_->config().preeditMode
                           : PreeditMode::No;

    switch (mode) {
    case PreeditMode::No:
        ic->inputPanel().setPreedit(preeditFromRimeContext(
            context, TextFormatFlag::NoFlag, TextFormatFlag::NoFlag));
        ic->inputPanel().setClientPreedit(Text());
        break;
    case PreeditMode::CommitPreview: {
        ic->inputPanel().setPreedit(preeditFromRimeContext(
            context, TextFormatFlag::NoFlag, TextFormatFlag::NoFlag));
        if (context.composition.length > 0 && context.commit_text_preview) {
            Text clientPreedit;
            clientPreedit.append(context.commit_text_preview,
                                 TextFormatFlag::Underline);
            if (*engine_->config().preeditCursorPositionAtBeginning) {
                clientPreedit.setCursor(0);
            } else {
                clientPreedit.setCursor(clientPreedit.textLength());
            }
            ic->inputPanel().setClientPreedit(clientPreedit);
        } else {
            ic->inputPanel().setClientPreedit(Text());
        }
    } break;
    case PreeditMode::ComposingText: {
        const TextFormatFlag highlightFlag =
            *engine_->config().preeditCursorPositionAtBeginning
                ? TextFormatFlag::HighLight
                : TextFormatFlag::NoFlag;
        Text clientPreedit = preeditFromRimeContext(
            context, TextFormatFlag::Underline, highlightFlag);
        if (*engine_->config().preeditCursorPositionAtBeginning) {
            clientPreedit.setCursor(0);
        }
        ic->inputPanel().setClientPreedit(clientPreedit);
    } break;
    }
}

void RimeState::updateUI(InputContext *ic, bool keyRelease) {
    auto &inputPanel = ic->inputPanel();
    if (!keyRelease) {
        inputPanel.reset();
    }
    bool oldEmptyExceptAux = emptyExceptAux(inputPanel);

    do {
        auto *api = engine_->api();
        if (api->is_maintenance_mode()) {
            return;
        }
        auto session = this->session();
        if (!api->find_session(session)) {
            return;
        }

        RIME_STRUCT(RimeContext, context);
        if (!api->get_context(session, &context)) {
            break;
        }

        updatePreedit(ic, context);

        if (context.menu.num_candidates) {
            ic->inputPanel().setCandidateList(
                std::make_unique<RimeCandidateList>(engine_, ic, context));
        } else {
            ic->inputPanel().setCandidateList(nullptr);
        }

        api->free_context(&context);
    } while (0);

    ic->updatePreedit();
    // HACK: for show input method information.
    // Since we don't use aux, which is great for this hack.
    bool newEmptyExceptAux = emptyExceptAux(inputPanel);
    // If it's key release and old information is not "empty", do the rest of
    // "reset".
    if (keyRelease && !newEmptyExceptAux) {
        inputPanel.setAuxUp(Text());
        inputPanel.setAuxDown(Text());
    }
    if (newEmptyExceptAux && lastMode_ != subMode()) {
        engine_->blockNotificationFor(30000);
        engine_->instance()->showInputMethodInformation(ic);
        ic->updateUserInterface(UserInterfaceComponent::StatusArea);
    }

    if (!keyRelease || !oldEmptyExceptAux || !newEmptyExceptAux) {
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
    }
}

void RimeState::release() { session_.reset(); }

void RimeState::commitPreedit(InputContext *ic) {
    if (auto *api = engine_->api()) {
        RIME_STRUCT(RimeContext, context);
        auto session = this->session();
        if (!api->get_context(session, &context)) {
            return;
        }
        if (context.composition.length > 0 && context.commit_text_preview) {
            ic->commitString(context.commit_text_preview);
        }
        api->free_context(&context);
    }
}

void RimeState::snapshot() {
    if (!session(false)) {
        return;
    }
    getStatus([this](const RimeStatus &status) {
        if (!status.schema_id) {
            return;
        }
        savedCurrentSchema_ = status.schema_id;
        savedOptions_.clear();
        if (savedCurrentSchema_.empty()) {
            return;
        }
        const auto &optionActions = engine_->optionActions();
        auto iter = optionActions.find(savedCurrentSchema_);
        if (iter == optionActions.end()) {
            return;
        }
        for (const auto &option : iter->second) {
            if (auto savedOption = option->snapshotOption(&ic_)) {
                savedOptions_.push_back(std::move(*savedOption));
            }
        }
    });
}

void RimeState::restore() {
    if (savedCurrentSchema_.empty()) {
        return;
    }
    if (!engine_->schemas().count(savedCurrentSchema_)) {
        return;
    }

    selectSchema(savedCurrentSchema_);
    for (const auto &option : savedOptions_) {
        if (stringutils::startsWith(option, "!")) {
            engine_->api()->set_option(session(), option.c_str() + 1, false);
        } else {
            engine_->api()->set_option(session(), option.c_str(), true);
        }
    }
}

} // namespace fcitx
