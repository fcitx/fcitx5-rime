/*
* Copyright (C) 2017~2017 by CSSlayer
* wengxt@gmail.com
*
* This library is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as
* published by the Free Software Foundation; either version 2.1 of the
* License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; see the file COPYING. If not,
* see <http://www.gnu.org/licenses/>.
*/

#include "rimestate.h"
#include "rimecandidate.h"
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
}

RimeState::RimeState(RimeEngine *engine) : engine_(engine) {
    if (auto api = engine_->api()) {
        session_ = api->create_session();
    }
}

RimeState::~RimeState() {
    if (auto api = engine_->api()) {
        api->destroy_session(session_);
    }
}

void RimeState::clear() {
    if (auto api = engine_->api()) {
        api->clear_composition(session_);
    }
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
    uint32_t states = event.key().states();
    if (event.isRelease()) {
        states |= (1 << 30);
    }
    auto result = api->process_key(session_, event.key().sym(), states);

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
    if (!api || !session_) {
        return false;
    }
    return api->get_status(session_, status);
}

void RimeState::updatePreedit(InputContext *ic, const RimeContext &context) {
    Text preedit;
    Text clientPreedit;

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
                                       context.composition.sel_start), TextFormatFlag::Underline);
            if (context.commit_text_preview) {
                clientPreedit.append(
                    std::string(context.commit_text_preview,
                                context.composition.sel_start), TextFormatFlag::Underline);
            }
        }

        /* converting candidate */
        if (context.composition.sel_start < context.composition.sel_end) {
            preedit.append(
                std::string(
                    &context.composition.preedit[context.composition.sel_start],
                    &context.composition.preedit[context.composition.sel_end]),
                TextFormatFlag::HighLight);
            if (context.commit_text_preview) {
                clientPreedit.append(
                    std::string(&context.commit_text_preview[context.composition
                                                                 .sel_start]),
                    TextFormatFlag::HighLight);
            }
        }

        /* remaining input to convert */
        if (context.composition.sel_end < context.composition.length) {
            preedit.append(std::string(
                &context.composition.preedit[context.composition.sel_end],
                &context.composition.preedit[context.composition.length]), TextFormatFlag::Underline);
        }

        preedit.setCursor(context.composition.cursor_pos);
    } while (0);
    ic->inputPanel().setPreedit(preedit);
    ic->inputPanel().setClientPreedit(clientPreedit);
}

void RimeState::updateUI(InputContext *ic, bool keyRelease) {
    auto &inputPanel = ic->inputPanel();
    if (!keyRelease) {
        inputPanel.reset();
    }
    bool oldEmptyExceptAux = emptyExceptAux(inputPanel);
    // FIXME: update status (button, menu)

    do {
        auto api = engine_->api();
        if (!api || api->is_maintenance_mode()) {
            return;
        }

        RIME_STRUCT(RimeContext, context);
        if (!api->get_context(session_, &context)) {
            break;
        }

        updatePreedit(ic, context);

        if (context.menu.num_candidates) {
            ic->inputPanel().setCandidateList(
                new RimeCandidateList(engine_, ic, context.menu));
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
    if (keyRelease && !emptyExceptAux(inputPanel)) {
        inputPanel.setAuxUp(Text());
        inputPanel.setAuxDown(Text());
    }

    if (!keyRelease || !oldEmptyExceptAux || !newEmptyExceptAux) {
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
    }
}
}
