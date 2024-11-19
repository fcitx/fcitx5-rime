/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */
#ifndef _FCITX5_RIME_RIMESERVICE_H_
#define _FCITX5_RIME_RIMESERVICE_H_

#include <fcitx-utils/dbus/message.h>
#include <fcitx-utils/dbus/objectvtable.h>

namespace fcitx::rime {

class RimeEngine;
class RimeState;

class RimeService : public dbus::ObjectVTable<RimeService> {
public:
    RimeService(RimeEngine *engine);

    void setAsciiMode(bool asciiMode);
    bool isAsciiMode();
    void setSchema(const std::string &schema);
    std::string currentSchema();
    std::vector<std::string> listAllSchemas();

private:
    RimeState *currentState();
    FCITX_OBJECT_VTABLE_METHOD(setAsciiMode, "SetAsciiMode", "b", "");
    FCITX_OBJECT_VTABLE_METHOD(isAsciiMode, "IsAsciiMode", "", "b");
    FCITX_OBJECT_VTABLE_METHOD(setSchema, "SetSchema", "s", "");
    FCITX_OBJECT_VTABLE_METHOD(currentSchema, "GetCurrentSchema", "", "s");
    FCITX_OBJECT_VTABLE_METHOD(listAllSchemas, "ListAllSchemas", "", "as");

    RimeEngine *engine_;
};

} // namespace fcitx::rime

#endif // _FCITX5_RIME_RIMESERVICE_H_
