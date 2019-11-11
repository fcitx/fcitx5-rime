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
#ifndef _FCITX_RIMESTATE_H_
#define _FCITX_RIMESTATE_H_

#include "rimeengine.h"
#include <fcitx/inputcontextproperty.h>

namespace fcitx {

class RimeState : public InputContextProperty {
public:
    RimeState(RimeEngine *engine);

    virtual ~RimeState();

    void clear();
    void keyEvent(KeyEvent &event);
    bool getStatus(RimeStatus *status);
    void updatePreedit(InputContext *ic, const RimeContext &context);
    void updateUI(InputContext *ic, bool keyRelease);
    void release();

private:
    RimeEngine *engine_;
    RimeSessionId session_ = 0;
};
} // namespace fcitx

#endif // _FCITX_RIMESTATE_H_
