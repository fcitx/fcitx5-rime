/*
 * SPDX-FileCopyrightText: 2020~2020 xzhao9 <i@xuzhao.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _RIME_GUI_CONFIG_PARSER_H
#define _RIME_GUI_CONFIG_PARSER_H

#include <functional>
#include <rime_levers_api.h>
#include <string>
#include <vector>

namespace fcitx::rime {

enum class KeyBindingCondition {
    Composing,
    HasMenu,
    Paging,
    Always,
};

enum class KeyBindingType {
    Send,
    Toggle,
    Select,
};

enum class SwitchKeyFunction {
    Noop,
    InlineASCII,
    CommitText,
    CommitCode,
    Clear,
};

struct KeyBinding {
    KeyBindingCondition when;
    std::string accept;
    KeyBindingType type;
    std::string action;
};

class RimeConfigParser {
public:
    RimeConfigParser();
    ~RimeConfigParser();

    bool isError();
    bool sync();

    void setSwitchKeys(const std::vector<SwitchKeyFunction> &switch_keys);
    std::vector<SwitchKeyFunction> getSwitchKeys();

    void setToggleKeys(const std::vector<std::string> &keys);
    std::vector<std::string> getToggleKeys();

    void setKeybindings(const std::vector<KeyBinding> &bindings);
    std::vector<KeyBinding> getKeybindings();

    void setPageSize(int page_size);
    bool getPageSize(int *page_size);

    std::string stringFromYAML(const char *yaml, const char *attr);
    void setSchemas(const std::vector<std::string> &schemas);
    int schemaIndex(const char *schema);

private:
    bool start(bool firstRun = false);
    static void
    listForeach(RimeConfig *config, const char *key,
                std::function<bool(RimeConfig *config, const char *path)>);

    RimeApi *api_;
    RimeLeversApi *levers_;
    RimeCustomSettings *settings_;
    RimeConfig defaultConf_;
    std::vector<std::string> schemaIdList_;
    bool inError_;
};

} // namespace fcitx::rime

#endif // _RIME_GUI_CONFIG_PARSER_H
