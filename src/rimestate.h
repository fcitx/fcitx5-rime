/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _FCITX_RIMESTATE_H_
#define _FCITX_RIMESTATE_H_

#include "rimesession.h"
#include <fcitx/globalconfig.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <memory>
#include <rime_api.h>

#define RIME_ASCII_MODE "ascii_mode"

namespace fcitx {

class RimeEngine;

class RimeState : public InputContextProperty {
public:
    RimeState(RimeEngine *engine, InputContext &ic);

    virtual ~RimeState();

    void clear();
    void keyEvent(KeyEvent &event);
#ifndef FCITX_RIME_NO_SELECT_CANDIDATE
    void selectCandidate(InputContext *inputContext, int idx);
#endif
    bool getStatus(const std::function<void(const RimeStatus &)> &);
    void updatePreedit(InputContext *ic, const RimeContext &context);
    void updateUI(InputContext *ic, bool keyRelease);
    void release();
    void commitPreedit(InputContext *ic);
    std::string subMode();
    std::string subModeLabel();
    void setLatinMode(bool latin);
    void selectSchema(const std::string &schemaId);
    RimeSessionId session(bool requestNewSession = true);

private:
    std::string lastMode_;
    RimeEngine *engine_;
    InputContext &ic_;
    std::shared_ptr<RimeSessionHolder> session_;
};
} // namespace fcitx

#endif // _FCITX_RIMESTATE_H_
