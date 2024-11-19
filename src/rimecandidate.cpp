/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "rimecandidate.h"
#include "rimeengine.h"
#include <cstring>
#include <fcitx-utils/log.h>
#include <fcitx/candidatelist.h>
#include <memory>
#include <rime_api.h>
#include <stdexcept>

namespace fcitx::rime {

RimeCandidateWord::RimeCandidateWord(RimeEngine *engine,
                                     const RimeCandidate &candidate, KeySym sym,
                                     int idx)
    : engine_(engine), sym_(sym), idx_(idx) {
    setText(Text{candidate.text});
    if (candidate.comment && candidate.comment[0]) {
        setComment(Text{candidate.comment});
    }
}

void RimeCandidateWord::select(InputContext *inputContext) const {
    if (auto *state = engine_->state(inputContext)) {
#ifndef FCITX_RIME_NO_SELECT_CANDIDATE
        state->selectCandidate(inputContext, idx_, /*global=*/false);
#else
        // Simulate the selection with a fake key event.
        KeyEvent event(inputContext, Key(sym_));
        state->keyEvent(event);
#endif
    }
}

void RimeCandidateWord::forget(RimeState *state) const {
#ifndef FCITX_RIME_NO_DELETE_CANDIDATE
    state->deleteCandidate(idx_, /*global=*/false);
#endif
}

#ifndef FCITX_RIME_NO_SELECT_CANDIDATE
RimeGlobalCandidateWord::RimeGlobalCandidateWord(RimeEngine *engine,
                                                 const RimeCandidate &candidate,
                                                 int idx)
    : engine_(engine), idx_(idx) {
    setText(Text{candidate.text});
    if (candidate.comment && candidate.comment[0]) {
        setComment(Text{candidate.comment});
    }
}

void RimeGlobalCandidateWord::select(InputContext *inputContext) const {
    if (auto *state = engine_->state(inputContext)) {
        state->selectCandidate(inputContext, idx_, /*global=*/true);
    }
}

void RimeGlobalCandidateWord::forget(RimeState *state) const {
#ifndef FCITX_RIME_NO_DELETE_CANDIDATE
    state->deleteCandidate(idx_, /*global=*/true);
#endif
}

#endif

RimeCandidateList::RimeCandidateList(RimeEngine *engine, InputContext *ic,
                                     const RimeContext &context)
    : engine_(engine), ic_(ic), hasPrev_(context.menu.page_no != 0),
      hasNext_(!context.menu.is_last_page) {
    setPageable(this);
    setBulk(this);
    setActionable(this);
#ifndef FCITX_RIME_NO_HIGHLIGHT_CANDIDATE
    setBulkCursor(this);
#endif

    const auto &menu = context.menu;

    int num_select_keys = menu.select_keys ? strlen(menu.select_keys) : 0;
    bool has_label = RIME_STRUCT_HAS_MEMBER(context, context.select_labels) &&
                     context.select_labels;

    int i;
    for (i = 0; i < menu.num_candidates; ++i) {
        KeySym sym = FcitxKey_None;
        std::string label;
        if (i < menu.page_size && has_label) {
            label = context.select_labels[i];
        } else if (i < num_select_keys) {
            label = std::string(1, menu.select_keys[i]);
        } else {
            label = std::to_string((i + 1) % 10);
        }
        label.append(" ");
        labels_.emplace_back(label);

        if (i < num_select_keys) {
            sym = static_cast<KeySym>(menu.select_keys[i]);
        } else {
            sym = static_cast<KeySym>('0' + (i + 1) % 10);
        }
        candidateWords_.emplace_back(std::make_unique<RimeCandidateWord>(
            engine, menu.candidates[i], sym, i));

        if (i == menu.highlighted_candidate_index) {
            cursor_ = i;
        }
    }
}

const CandidateWord &RimeCandidateList::candidateFromAll(int idx) const {
    if (idx < 0 || empty()) {
        throw std::invalid_argument("Invalid global index");
    }

    auto session = engine_->state(ic_)->session(false);
    if (!session) {
        throw std::invalid_argument("Invalid session");
    }

    auto index = static_cast<size_t>(idx);

    auto *api = engine_->api();

    RimeCandidateListIterator iter;
    if (index >= globalCandidateWords_.size()) {
        if (index >= maxSize_) {
            throw std::invalid_argument("Invalid global index");
        }
    } else {
        if (globalCandidateWords_[index]) {
            return *globalCandidateWords_[index];
        }
    }

    if (!api->candidate_list_from_index(session, &iter, idx) ||
        !api->candidate_list_next(&iter)) {
        maxSize_ = std::min(index, maxSize_);
        throw std::invalid_argument("Invalid global index");
    }

    if (index >= globalCandidateWords_.size()) {
        globalCandidateWords_.resize(index + 1);
    }
    globalCandidateWords_[index] =
        std::make_unique<RimeGlobalCandidateWord>(engine_, iter.candidate, idx);
    api->candidate_list_end(&iter);
    return *globalCandidateWords_[index];
}

int RimeCandidateList::totalSize() const { return -1; }

bool RimeCandidateList::hasAction(const CandidateWord & /*candidate*/) const {
#ifndef FCITX_RIME_NO_DELETE_CANDIDATE
    // We can always reset rime candidate's frequency.
    return true;
#else
    return false;
#endif
}

std::vector<CandidateAction>
RimeCandidateList::candidateActions(const CandidateWord & /*candidate*/) const {
    std::vector<CandidateAction> actions;
#ifndef FCITX_RIME_NO_DELETE_CANDIDATE
    CandidateAction action;
    action.setId(0);
    action.setText(_("Forget word"));
    actions.push_back(std::move(action));
#endif
    return actions;
}

void RimeCandidateList::triggerAction(const CandidateWord &candidate, int id) {
    if (id != 0) {
        return;
    }
    if (auto state = engine_->state(ic_)) {
        if (const auto *rimeCandidate =
                dynamic_cast<const RimeGlobalCandidateWord *>(&candidate)) {
            rimeCandidate->forget(state);
        } else if (const auto *rimeCandidate =
                       dynamic_cast<const RimeCandidateWord *>(&candidate)) {
            rimeCandidate->forget(state);
        }
    }
}

#ifndef FCITX_RIME_NO_HIGHLIGHT_CANDIDATE
int RimeCandidateList::globalCursorIndex() const {
    return -1; // No API available.
}

void RimeCandidateList::setGlobalCursorIndex(int index) {
    auto session = engine_->state(ic_)->session(false);
    if (!session) {
        return;
    }
    auto *api = engine_->api();
    api->highlight_candidate(session, index);
}
#endif
} // namespace fcitx::rime
