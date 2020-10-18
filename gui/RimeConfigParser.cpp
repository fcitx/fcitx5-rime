/*
 * SPDX-FileCopyrightText: 2020~2020 xzhao9 <i@xuzhao.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "RimeConfigParser.h"
#include <QDebug>
#include <fcitx-utils/standardpath.h>
#include <fcitx-utils/stringutils.h>
#include <string>
#include <string_view>

namespace fcitx::rime {

RimeConfigParser::RimeConfigParser()
    : api_(rime_get_api()), defaultConf_({0}), inError_(false) {
    RimeModule *module = api_->find_module("levers");
    if (!module) {
        inError_ = true;
        return;
    }
    levers_ = (RimeLeversApi *)module->get_api();
    start(true);
}

RimeConfigParser::~RimeConfigParser() { api_->finalize(); }

bool RimeConfigParser::isError() { return inError_; }

bool RimeConfigParser::start(bool firstRun) {
    std::string userPath;

    userPath = stringutils::joinPath(
        StandardPath::global().userDirectory(StandardPath::Type::PkgData),
        "rime");

    RIME_STRUCT(RimeTraits, fcitx_rime_traits);
    fcitx_rime_traits.shared_data_dir = RIME_DATA_DIR;
    fcitx_rime_traits.user_data_dir = userPath.c_str();
    fcitx_rime_traits.distribution_name = "Rime";
    fcitx_rime_traits.distribution_code_name = "fcitx-rime-config";
    fcitx_rime_traits.distribution_version = FCITX_RIME_VERSION;
    fcitx_rime_traits.app_name = "rime.fcitx-rime-config";
    if (firstRun) {
        api_->setup(&fcitx_rime_traits);
    }
    defaultConf_ = {0};
    api_->initialize(&fcitx_rime_traits);
    settings_ = levers_->custom_settings_init("default", "rime_patch");
    levers_->load_settings(settings_);
    levers_->settings_get_config(settings_, &defaultConf_);
    return true;
}

void RimeConfigParser::setToggleKeys(const std::vector<std::string> &keys) {
    api_->config_clear(&defaultConf_, "switcher/hotkeys");
    api_->config_create_list(&defaultConf_, "switcher/hotkeys");
    RimeConfigIterator iterator;
    api_->config_begin_list(&iterator, &defaultConf_, "switcher/hotkeys");
    api_->config_next(&iterator);
    for (size_t i = 0; i < keys.size(); i++) {
        api_->config_next(&iterator);
        api_->config_set_string(&defaultConf_, iterator.path, keys[i].data());
    }
    api_->config_end(&iterator);
}

std::vector<std::string> RimeConfigParser::getToggleKeys() {
    std::vector<std::string> result;
    listForeach(&defaultConf_, "switcher/hotkeys",
                [=, &result](RimeConfig *config, const char *path) {
                    auto str = api_->config_get_cstring(config, path);
                    if (str) {
                        result.push_back(str);
                    }
                    return true;
                });
    return result;
}

static const char *keyBindingConditionToString(KeyBindingCondition condition) {
    switch (condition) {
    case KeyBindingCondition::Composing:
        return "composing";
    case KeyBindingCondition::HasMenu:
        return "has_menu";
    case KeyBindingCondition::Always:
        return "always";
    case KeyBindingCondition::Paging:
        return "paging";
    }
    return "";
}

static KeyBindingCondition keyBindingConditionFromString(std::string_view str) {
    if (str == "composing") {
        return KeyBindingCondition::Composing;
    } else if (str == "has_menu") {
        return KeyBindingCondition::HasMenu;
    } else if (str == "paging") {
        return KeyBindingCondition::Paging;
    } else if (str == "always") {
        return KeyBindingCondition::Always;
    }
    return KeyBindingCondition::Composing;
}

static const char *keybindingTypeToString(KeyBindingType type) {
    switch (type) {
    case KeyBindingType::Send:
        return "send";
    case KeyBindingType::Select:
        return "select";
    case KeyBindingType::Toggle:
        return "toggle";
    }
    return "";
}

static const char *switchKeyFunctionToString(SwitchKeyFunction type) {
    switch (type) {
    case SwitchKeyFunction::Noop:
        return "noop";
    case SwitchKeyFunction::InlineASCII:
        return "inline_ascii";
    case SwitchKeyFunction::CommitText:
        return "commit_text";
    case SwitchKeyFunction::CommitCode:
        return "commit_code";
    case SwitchKeyFunction::Clear:
        return "clear";
    }
    return "";
}

static SwitchKeyFunction switchKeyFunctionFromString(std::string_view str) {
    if (str == "noop") {
        return SwitchKeyFunction::Noop;
    } else if (str == "inline_ascii") {
        return SwitchKeyFunction::InlineASCII;
    } else if (str == "commit_text") {
        return SwitchKeyFunction::CommitText;
    } else if (str == "commit_code") {
        return SwitchKeyFunction::CommitCode;
    } else if (str == "clear") {
        return SwitchKeyFunction::Clear;
    }
    return SwitchKeyFunction::Noop;
}

void RimeConfigParser::setKeybindings(const std::vector<KeyBinding> &bindings) {
    RimeConfig copyConfig = {0};
    RimeConfig copyConfigMap = {0};
    RimeConfigIterator iterator;
    RimeConfigIterator copyIterator;
    api_->config_init(&copyConfig);
    api_->config_create_list(&copyConfig, "key_binder/bindings");
    api_->config_begin_list(&iterator, &defaultConf_, "key_binder/bindings");
    api_->config_begin_list(&copyIterator, &copyConfig, "key_binder/bindings");
    while (!copyIterator.path) {
        api_->config_next(&copyIterator);
    }
    while (api_->config_next(&iterator)) {
        RimeConfig map = {0};
        const char *sendKey = nullptr;
        api_->config_get_item(&defaultConf_, iterator.path, &map);
        sendKey = api_->config_get_cstring(&map, "send");
        if (!sendKey) {
            sendKey = api_->config_get_cstring(&map, "toggle");
        }
        if (!sendKey) {
            sendKey = api_->config_get_cstring(&map, "select");
        }
        if (strcmp(sendKey, "Page_Up") && strcmp(sendKey, "Page_Down") &&
            strcmp(sendKey, "ascii_mode") && strcmp(sendKey, "full_shape") &&
            strcmp(sendKey, "simplification")) {
            api_->config_set_item(&copyConfig, copyIterator.path, &map);
            api_->config_next(&copyIterator);
        }
    };
    api_->config_end(&iterator);
    for (auto &binding : bindings) {
        RimeConfig map = {0};
        api_->config_init(&map);
        api_->config_set_string(&map, "accept", binding.accept.data());
        api_->config_set_string(&map, "when",
                                keyBindingConditionToString(binding.when));
        api_->config_set_string(&map, keybindingTypeToString(binding.type),
                                binding.action.data());
        api_->config_set_item(&copyConfig, copyIterator.path, &map);
        api_->config_next(&copyIterator);
    }
    api_->config_end(&copyIterator);
    api_->config_get_item(&copyConfig, "key_binder/bindings", &copyConfigMap);
    api_->config_set_item(&defaultConf_, "key_binder/bindings", &copyConfigMap);
}

void RimeConfigParser::setPageSize(int pageSize) {
    api_->config_set_int(&defaultConf_, "menu/page_size", pageSize);
}

bool RimeConfigParser::getPageSize(int *pageSize) {
    return api_->config_get_int(&defaultConf_, "menu/page_size", pageSize);
}

std::vector<KeyBinding> RimeConfigParser::getKeybindings() {
    std::vector<KeyBinding> result;
    listForeach(&defaultConf_, "key_binder/bindings",
                [=, &result](RimeConfig *config, const char *path) {
                    RimeConfig map = {0};
                    api_->config_get_item(config, path, &map);
                    auto when = api_->config_get_cstring(&map, "when");
                    if (!when) {
                        return false;
                    }
                    KeyBinding binding;
                    binding.when = keyBindingConditionFromString(when);
                    auto accept = api_->config_get_cstring(&map, "accept");
                    if (!accept) {
                        return false;
                    }
                    binding.accept = accept;
                    auto action = api_->config_get_cstring(&map, "send");
                    if (action) {
                        binding.type = KeyBindingType::Send;
                    } else {
                        action = api_->config_get_cstring(&map, "toggle");
                    }
                    if (action) {
                        binding.type = KeyBindingType::Toggle;
                    } else {
                        action = api_->config_get_cstring(&map, "select");
                        binding.type = KeyBindingType::Select;
                    }
                    if (!action) {
                        return false;
                    }
                    binding.action = action;
                    result.push_back(std::move(binding));
                    return true;
                });
    return result;
}

void RimeConfigParser::listForeach(
    RimeConfig *config, const char *key,
    std::function<bool(RimeConfig *, const char *)> callback) {
    size_t size = RimeConfigListSize(config, key);
    if (!size) {
        return;
    }

    RimeConfigIterator iterator;
    RimeConfigBeginList(&iterator, config, key);
    for (size_t i = 0; i < size; i++) {
        RimeConfigNext(&iterator);
        if (!callback(config, iterator.path)) {
            break;
        }
    }
    RimeConfigEnd(&iterator);
}

bool RimeConfigParser::sync() {
    bool suc;
    RimeConfig hotkeys = {0};
    RimeConfig keybindings = {0};
    RimeConfig schema_list = {0};
    std::string yaml;

    int pageSize;
    api_->config_get_int(&defaultConf_, "menu/page_size", &pageSize);
    levers_->customize_int(settings_, "menu/page_size", pageSize);
    api_->config_get_item(&defaultConf_, "switcher/hotkeys", &hotkeys);
    levers_->customize_item(settings_, "switcher/hotkeys", &hotkeys);
    api_->config_get_item(&defaultConf_, "key_binder/bindings", &keybindings);
    levers_->customize_item(settings_, "key_binder/bindings", &keybindings);
    levers_->customize_string(
        settings_, "ascii_composer/switch_key/Shift_L",
        api_->config_get_cstring(&defaultConf_,
                                 "ascii_composer/switch_key/Shift_L"));
    levers_->customize_string(
        settings_, "ascii_composer/switch_key/Shift_R",
        api_->config_get_cstring(&defaultConf_,
                                 "ascii_composer/switch_key/Shift_R"));

    /* Concatenate all active schemas */
    for (const auto &schema : schemaIdList_) {
        yaml += "- { schema: " + schema + " } \n";
    }
    api_->config_load_string(&schema_list, yaml.c_str());
    levers_->customize_item(settings_, "schema_list", &schema_list);
    suc = levers_->save_settings(settings_);
    if (!suc) {
        return false;
    }
    levers_->custom_settings_destroy(settings_);
    suc = api_->start_maintenance(true); // Full check mode
    if (!suc) {
        return false;
    }
    api_->finalize();
    return start(false);
}

std::string RimeConfigParser::stringFromYAML(const char *yaml,
                                             const char *attr) {
    RimeConfig rimeSchemaConfig = {0};
    api_->config_load_string(&rimeSchemaConfig, yaml);
    auto str = api_->config_get_cstring(&rimeSchemaConfig, attr);
    std::string result;
    if (str) {
        result = str;
    }
    return result;
}

void RimeConfigParser::setSchemas(const std::vector<std::string> &schemas) {
    schemaIdList_ = schemas;
    return;
}

int RimeConfigParser::schemaIndex(const char *schema_id) {
    int idx = 0;
    bool found = false;
    listForeach(&defaultConf_, "schema_list",
                [=, &idx, &found](RimeConfig *config, const char *path) {
                    RimeConfig map = {0};
                    this->api_->config_get_item(config, path, &map);
                    auto schema =
                        this->api_->config_get_cstring(&map, "schema");
                    /* This schema is enabled in default */
                    if (schema && strcmp(schema, schema_id) == 0) {
                        found = true;
                        return false;
                    }
                    idx++;
                    return true;
                });

    return found ? (idx + 1) : 0;
}

std::vector<SwitchKeyFunction> RimeConfigParser::getSwitchKeys() {
    std::vector<SwitchKeyFunction> out;
    const char *shiftL = nullptr, *shiftR = nullptr;
    shiftL = api_->config_get_cstring(&defaultConf_,
                                      "ascii_composer/switch_key/Shift_L");
    shiftR = api_->config_get_cstring(&defaultConf_,
                                      "ascii_composer/switch_key/Shift_R");
    out.push_back(switchKeyFunctionFromString(shiftL));
    out.push_back(switchKeyFunctionFromString(shiftR));
    return out;
}

void RimeConfigParser::setSwitchKeys(
    const std::vector<SwitchKeyFunction> &switch_keys) {
    if (switch_keys.size() < 2) {
        return;
    }
    api_->config_set_string(&defaultConf_, "ascii_composer/switch_key/Shift_L",
                            switchKeyFunctionToString(switch_keys[0]));
    api_->config_set_string(&defaultConf_, "ascii_composer/switch_key/Shift_R",
                            switchKeyFunctionToString(switch_keys[1]));
    return;
}

} // namespace fcitx::rime
