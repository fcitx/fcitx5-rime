/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _FCITX_RIMEACTION_H_
#define _FCITX_RIMEACTION_H_

#include <fcitx/action.h>
#include <fcitx/inputcontext.h>
#include <fcitx/menu.h>
#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fcitx {

class RimeEngine;

class RimeOptionAction : public Action {
public:
    // This is used to save the option when we need to release the session.
    virtual std::optional<std::string> snapshotOption(InputContext *ic) = 0;
    // Return the label of current option.
    virtual std::string optionLabel(InputContext* ic) = 0;
    // Check whether a option name belongs to the action.
    virtual bool checkOptionName(std::string_view name) const = 0;
};

class ToggleAction : public RimeOptionAction {
public:
    ToggleAction(RimeEngine *engine, std::string_view schema,
                 std::string_view option, std::string disabledText,
                 std::string enabledText);

    void activate(InputContext *ic) override;

    std::string shortText(InputContext *ic) const override;

    std::string icon(InputContext * /*unused*/) const override { return ""; }

    std::optional<std::string> snapshotOption(InputContext *ic) override;

    const std::string &option() const { return option_; }

    std::string optionLabel(InputContext *ic) override;

    bool checkOptionName(std::string_view name) const override;

private:
    RimeEngine *engine_;
    std::string option_;
    std::string disabledText_;
    std::string enabledText_;
};

class SelectAction : public RimeOptionAction {
public:
    SelectAction(RimeEngine *engine, std::string_view schema,
                 std::vector<std::string> options,
                 std::vector<std::string> texts);

    std::string shortText(InputContext *ic) const override;

    std::string icon(InputContext * /*unused*/) const override { return ""; }

    std::optional<std::string> snapshotOption(InputContext *ic) override;

    const std::vector<std::string> &options() const { return options_; }

    std::string optionLabel(InputContext *ic) override;

    bool checkOptionName(std::string_view name) const override;

private:
    RimeEngine *engine_;
    std::vector<std::string> options_;
    std::vector<std::string> texts_;
    std::list<SimpleAction> actions_;
    Menu menu_;
};

} // namespace fcitx

#endif // _FCITX_RIMEENGINE_H_
