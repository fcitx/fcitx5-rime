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
#ifndef _FCITX_RIMECANDIDATE_H_
#define _FCITX_RIMECANDIDATE_H_

#include "rimeengine.h"
#include "rimestate.h"
#include <fcitx/candidatelist.h>

namespace fcitx {

class RimeCandidateWord : public CandidateWord {
public:
    RimeCandidateWord(RimeEngine *engine, const RimeCandidate &candidate,
                      KeySym sym);

    void select(InputContext *inputContext) const override;

private:
    RimeEngine *engine_;
    KeySym sym_;
};

class RimeCandidateList final : public CandidateList,
                                public PageableCandidateList {
public:
    RimeCandidateList(RimeEngine *engine, InputContext *ic,
                      const RimeContext &context);

    const Text &label(int idx) const override {
        checkIndex(idx);
        return labels_[idx];
    }

    const CandidateWord &candidate(int idx) const override {
        checkIndex(idx);
        return *candidateWords_[idx];
    }
    int size() const override { return candidateWords_.size(); }

    int cursorIndex() const override { return cursor_; }

    CandidateLayoutHint layoutHint() const override { return layout_; }

    bool hasPrev() const override { return hasPrev_; }
    bool hasNext() const override { return hasNext_; }
    void prev() override {
        KeyEvent event(ic_, Key(FcitxKey_Page_Up));
        engine_->state(ic_)->keyEvent(event);
    }
    void next() override {
        KeyEvent event(ic_, Key(FcitxKey_Page_Down));
        engine_->state(ic_)->keyEvent(event);
    }

    bool usedNextBefore() const override { return true; }

private:
    void checkIndex(int idx) const {
        if (idx < 0 && idx >= size()) {
            throw std::invalid_argument("invalid index");
        }
    }

    RimeEngine *engine_;
    InputContext *ic_;
    std::vector<Text> labels_;
    bool hasPrev_ = false;
    bool hasNext_ = false;
    CandidateLayoutHint layout_ = CandidateLayoutHint::NotSet;
    int cursor_ = -1;
    std::vector<std::unique_ptr<CandidateWord>> candidateWords_;
};
} // namespace fcitx

#endif // _FCITX_RIMECANDIDATE_H_
