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

#include "rimecandidate.h"
#include <cstring>

namespace fcitx {

RimeCandidateWord::RimeCandidateWord(RimeEngine *engine,
                                     const RimeCandidate &candidate, KeySym sym)
    : CandidateWord(), engine_(engine), sym_(sym) {
    Text text;
    text.append(std::string(candidate.text));
    if (candidate.comment && strlen(candidate.comment)) {
        text.append(" ");
        text.append(std::string(candidate.comment));
    }
    setText(text);
}

void RimeCandidateWord::select(InputContext *inputContext) const {
    // Rime does not provide such an API, simulate the selection with a fake
    // key event.
    auto state = engine_->state(inputContext);
    KeyEvent event(inputContext, Key(sym_));
    state->keyEvent(event);
}

RimeCandidateList::RimeCandidateList(RimeEngine *engine, InputContext *ic,
                                     const RimeContext &context)
    : engine_(engine), ic_(ic), hasPrev_(context.menu.page_no != 0),
      hasNext_(!context.menu.is_last_page) {
    setPageable(this);

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
            engine, menu.candidates[i], sym));

        if (i == menu.highlighted_candidate_index) {
            cursor_ = i;
        }
    }
}
} // namespace fcitx
