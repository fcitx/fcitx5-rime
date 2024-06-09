/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "rimestate.h"
#include "rimeaction.h"
#include "rimecandidate.h"
#include "rimeengine.h"
#include "rimesession.h"
#include <algorithm>
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
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/text.h>
#include <fcitx/userinterface.h>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <optional>
#include <rime_api.h>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

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

void RimeState::activate() { maybeSyncProgramNameToSession(); }

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

std::string RimeState::currentSchema() {
    std::string schema;
    getStatus([&schema](const RimeStatus &status) {
        if (status.schema_id) {
            schema = status.schema_id;
        }
    });
    return schema;
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
    changedOptions_.clear();
    auto *ic = event.inputContext();
    // For key-release, composeResult will always be empty string, which feed
    // into engine directly.
    std::string composeResult;
    if (!event.key().states().testAny(
            KeyStates{KeyState::Ctrl, KeyState::Super}) &&
        !event.isRelease()) {
        auto compose =
            engine_->instance()->processComposeString(&ic_, event.key().sym());
        if (!compose) {
            event.filterAndAccept();
            return;
        }
        composeResult = *compose;
    }

    auto *api = engine_->api();
    if (api->is_maintenance_mode()) {
        return;
    }
    auto session = this->session();
    if (!session) {
        return;
    }

    maybeSyncProgramNameToSession();
    lastMode_ = subMode();

    std::string lastSchema = currentSchema();
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
    if (!composeResult.empty()) {
        event.filterAndAccept();
        auto length = utf8::lengthValidated(composeResult);
        bool result = false;
        if (length == 1) {
            auto c = utf8::getChar(composeResult);
            auto sym = Key::keySymFromUnicode(c);
            if (sym != FcitxKey_None) {
                result = api->process_key(session, sym, intStates);
            }
        }
        if (!result) {
            commitPreedit(ic);
            ic->commitString(composeResult);
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
    if (!event.isRelease() && !lastSchema.empty() &&
        lastSchema == currentSchema() && ic->inputPanel().empty() &&
        !changedOptions_.empty()) {
        showChangedOptions();
    }
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

#ifndef FCITX_RIME_NO_DELETE_CANDIDATE
void RimeState::deleteCandidate(int idx, bool global) {
    auto *api = engine_->api();
    if (api->is_maintenance_mode()) {
        return;
    }
    auto session = this->session();
    if (!session) {
        return;
    }
    if (global) {
        api->delete_candidate(session, idx);
    } else {
        api->delete_candidate_on_current_page(session, idx);
    }
    updateUI(&ic_, false);
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
    } while (false);

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

        savedOptions_ = snapshotOptions(savedCurrentSchema_);
    });
}

std::vector<std::string> RimeState::snapshotOptions(const std::string &schema) {
    if (schema.empty()) {
        return {};
    }
    std::vector<std::string> savedOptions;
    const auto &optionActions = engine_->optionActions();
    auto iter = optionActions.find(schema);
    if (iter == optionActions.end()) {
        return {};
    }
    for (const auto &option : iter->second) {
        if (auto savedOption = option->snapshotOption(&ic_)) {
            savedOptions.push_back(std::move(*savedOption));
        }
    }
    return savedOptions;
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

void RimeState::maybeSyncProgramNameToSession() {
    // The program name is guranteed to be const through the Input Context
    // lifetime. There is no need to update it if the policy is not "All".
    if (engine_->sessionPool().propertyPropagatePolicy() !=
        PropertyPropagatePolicy::All) {
        return;
    }

    if (session_) {
        session_->setProgramName(ic_.program());
    }
}

void RimeState::addChangedOption(std::string_view option) {
    changedOptions_.push_back(std::string(option));
}
void RimeState::showChangedOptions() {

    std::string schema = currentSchema();
    if (schema.empty()) {
        return;
    }
    const auto &optionActions = engine_->optionActions();
    auto iter = optionActions.find(schema);
    if (iter == optionActions.end()) {
        return;
    }
    const auto &actions = iter->second;

    std::string labels;
    std::unordered_set<RimeOptionAction *> actionSet;
    std::vector<RimeOptionAction *> actionList;

    auto extractOptionName = [](std::string_view &option) {
        const bool state = (option.front() != '!');
        if (!state) {
            option.remove_prefix(1);
        }
        return state;
    };

    for (std::string_view option : changedOptions_) {
        if (option.empty()) {
            continue;
        }
        extractOptionName(option);
        // Skip internal options.
        if (stringutils::startsWith(option, "_")) {
            continue;
        }

        // This is hard coded latin-mode.
        if (option == "ascii_mode") {
            continue;
        }

        // Filter by action, so we know this option belongs to current schema.
        auto actionIter = std::find_if(
            actions.begin(), actions.end(),
            [option](const std::unique_ptr<RimeOptionAction> &action) {
                return action->checkOptionName(option);
            });
        if (actionIter == actions.end()) {
            continue;
        }
        if (actionSet.count(actionIter->get())) {
            continue;
        }
        actionSet.insert(actionIter->get());
        actionList.push_back(actionIter->get());
    }

    for (auto *action : actionList) {
        // Snapshot again, so SelectAction will return the current active value.
        auto snapshot = action->snapshotOption(&ic_);
        if (!snapshot) {
            continue;
        }
        std::string_view option = *snapshot;
        const bool state = extractOptionName(option);
        auto label = engine_->api()->get_state_label_abbreviated(
            session(), option.data(), state, true);
        if (!label.str || label.length <= 0) {
            continue;
        }
        labels.append(label.str, label.length);
    }
    if (!labels.empty()) {
        engine_->instance()->showCustomInputMethodInformation(&ic_, labels);
    }
}
} // namespace fcitx
