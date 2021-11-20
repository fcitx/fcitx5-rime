/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "rimestate.h"
#include "rimecandidate.h"
#include <fcitx-utils/utf8.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>

namespace fcitx {

namespace {

bool emptyExceptAux(const InputPanel &inputPanel) {

    return inputPanel.preedit().size() == 0 &&
           inputPanel.preedit().size() == 0 &&
           (!inputPanel.candidateList() ||
            inputPanel.candidateList()->size() == 0);
}
} // namespace

RimeState::RimeState(RimeEngine *engine) : engine_(engine) {
    if (auto api = engine_->api()) {
        session_ = api->create_session();
    }
}

RimeState::~RimeState() {
    if (auto api = engine_->api()) {
        if (session_) {
            api->destroy_session(session_);
        }
    }
}

void RimeState::clear() {
    if (auto api = engine_->api()) {
        if (session_) {
            api->clear_composition(session_);
        }
    }
}

std::string RimeState::subMode() {
    std::string result;
    RIME_STRUCT(RimeStatus, status);
    if (getStatus(&status)) {
        if (status.is_disabled) {
            result = "\xe2\x8c\x9b";
        } else if (status.is_ascii_mode) {
            result = _("Latin Mode");
        } else if (status.schema_name && status.schema_name[0] != '.') {
            result = status.schema_name;
        }
        engine_->api()->free_status(&status);
    }
    return result;
}

std::string RimeState::subModeLabel() {
    std::string result;
    RIME_STRUCT(RimeStatus, status);
    if (getStatus(&status)) {
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
        engine_->api()->free_status(&status);
    }
    return result;
}

void RimeState::setLatinMode(bool latin) {
    auto api = engine_->api();
    if (!api || api->is_maintenance_mode()) {
        return;
    }
    api->set_option(session_, "ascii_mode", latin);
}

void RimeState::selectSchema(const std::string &schema) {
    auto api = engine_->api();
    if (!api || api->is_maintenance_mode()) {
        return;
    }
    api->set_option(session_, "ascii_mode", false);
    api->select_schema(session_, schema.data());
}

void RimeState::keyEvent(KeyEvent &event) {
    auto api = engine_->api();
    if (!api || api->is_maintenance_mode()) {
        return;
    }
    if (!api->find_session(session_)) {
        session_ = api->create_session();
    }
    if (!session_) {
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
    auto result = api->process_key(session_, event.rawKey().sym(), intStates);

    auto ic = event.inputContext();
    RIME_STRUCT(RimeCommit, commit);
    if (api->get_commit(session_, &commit)) {
        ic->commitString(commit.text);
        api->free_commit(&commit);
    }

    updateUI(ic, event.isRelease());

    if (result) {
        event.filterAndAccept();
    }
}

bool RimeState::getStatus(RimeStatus *status) {
    auto api = engine_->api();
    if (!api) {
        return false;
    }
    if (!api->find_session(session_)) {
        session_ = api->create_session();
    }
    if (!session_) {
        return false;
    }
    return api->get_status(session_, status);
}

void RimeState::updatePreedit(InputContext *ic, const RimeContext &context) {
    Text preedit;
    Text clientPreedit;

    TextFormatFlags flag;
    if (engine_->config().showPreeditInApplication.value() &&
        ic->capabilityFlags().test(CapabilityFlag::Preedit)) {
        flag = TextFormatFlag::Underline;
    }
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
            if (context.commit_text_preview) {
                clientPreedit.append(std::string(context.commit_text_preview,
                                                 context.composition.sel_start),
                                     flag);
            }
        }

        /* converting candidate */
        if (context.composition.sel_start < context.composition.sel_end) {
            preedit.append(
                std::string(
                    &context.composition.preedit[context.composition.sel_start],
                    &context.composition.preedit[context.composition.sel_end]),
                flag | TextFormatFlag::HighLight);
            if (context.commit_text_preview) {
                clientPreedit.append(
                    std::string(&context.commit_text_preview[context.composition
                                                                 .sel_start]),
                    flag | TextFormatFlag::HighLight);
            }
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

    if (engine_->config().showPreeditInApplication.value() &&
        ic->capabilityFlags().test(CapabilityFlag::Preedit)) {
        clientPreedit = preedit;
    } else {
        ic->inputPanel().setPreedit(preedit);
    }
    if (engine_->config().preeditCursorPositionAtBeginning.value()) {
        clientPreedit.setCursor(0);
    }
    ic->inputPanel().setClientPreedit(clientPreedit);
}

void RimeState::updateUI(InputContext *ic, bool keyRelease) {
    auto &inputPanel = ic->inputPanel();
    if (!keyRelease) {
        inputPanel.reset();
    }
    bool oldEmptyExceptAux = emptyExceptAux(inputPanel);
    engine_->updateAction(ic);

    do {
        auto api = engine_->api();
        if (!api || api->is_maintenance_mode()) {
            return;
        }
        if (!api->find_session(session_)) {
            return;
        }

        RIME_STRUCT(RimeContext, context);
        if (!api->get_context(session_, &context)) {
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
        engine_->instance()->showInputMethodInformation(ic);
        ic->updateUserInterface(UserInterfaceComponent::StatusArea);
    }

    if (!keyRelease || !oldEmptyExceptAux || !newEmptyExceptAux) {
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
    }
}

void RimeState::release() {
    if (auto api = engine_->api()) {
        if (session_) {
            api->destroy_session(session_);
        }
        session_ = 0;
    }
}

void RimeState::commitPreedit(InputContext *ic) {
    if (auto api = engine_->api()) {
        RIME_STRUCT(RimeContext, context);
        if (!api->get_context(session_, &context)) {
            return;
        }
        if (context.commit_text_preview) {
            ic->commitString(context.commit_text_preview);
        }
    }
}

} // namespace fcitx
