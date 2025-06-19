/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _FCITX_RIMESTATE_H_
#define _FCITX_RIMESTATE_H_

#include "rimesession.h"
#include <fcitx/event.h>
#include <fcitx/globalconfig.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <functional>
#include <memory>
#include <rime_api.h>
#include <string>
#include <string_view>
#include <vector>

#define RIME_ASCII_MODE "ascii_mode"
#define FCITX_HIDE_CANDIDATES "_hide_candidate"

namespace fcitx::rime {

class RimeEngine;

class RimeState : public InputContextProperty {
public:
    RimeState(RimeEngine *engine, InputContext &ic);

    virtual ~RimeState();

    void clear();
    void activate();
    void keyEvent(KeyEvent &event);
    void selectCandidate(InputContext *inputContext, int idx, bool global);
#ifndef FCITX_RIME_NO_DELETE_CANDIDATE
    void deleteCandidate(int idx, bool global);
#endif
    bool getStatus(const std::function<void(const RimeStatus &)> &);
    void updatePreedit(InputContext *ic, const RimeContext &context);
    void updateUI(InputContext *ic, bool keyRelease);
    void release();
    void commitInput(InputContext *ic);
    void commitComposing(InputContext *ic);
    void commitPreedit(InputContext *ic);
    std::string subMode();
    std::string subModeLabel();
    void toggleLatinMode();
    void setLatinMode(bool latin);
    void selectSchema(const std::string &schemaId);
    RimeSessionId session(bool requestNewSession = true);

    void snapshot();
    void restore();
    std::string currentSchema();
    void addChangedOption(std::string_view option);
    void showChangedOptions();

private:
    void maybeSyncProgramNameToSession();
    std::vector<std::string> snapshotOptions(const std::string &schema);

    std::string lastMode_;
    RimeEngine *engine_;
    InputContext &ic_;
    std::shared_ptr<RimeSessionHolder> session_;

    std::string savedCurrentSchema_;
    std::vector<std::string> savedOptions_;
    std::vector<std::string> changedOptions_;
};
} // namespace fcitx::rime

#endif // _FCITX_RIMESTATE_H_
