/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "rimeengine.h"
#include "notifications_public.h"
#include "rimestate.h"
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/fs.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/standardpath.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/statusarea.h>
#include <fcitx/userinterface.h>
#include <fcitx/userinterfacemanager.h>
#include <rime_api.h>
#include <stdexcept>
#include <string>

FCITX_DEFINE_LOG_CATEGORY(rime, "rime");

namespace fcitx {

namespace {

std::unordered_map<std::string, std::unordered_map<std::string, bool>>
parseAppOptions(rime_api_t *api, RimeConfig *config) {
    std::unordered_map<std::string, std::unordered_map<std::string, bool>>
        appOptions;
    RimeConfigIterator appIter;
    RimeConfigIterator optionIter;
    api->config_begin_map(&appIter, config, "app_options");
    while (api->config_next(&appIter)) {
        auto &options = appOptions[appIter.key];
        api->config_begin_map(&optionIter, config, appIter.path);
        while (api->config_next(&optionIter)) {
            Bool value = False;
            if (api->config_get_bool(config, optionIter.path, &value)) {
                options[optionIter.key] = !!value;
            }
        }
        api->config_end(&optionIter);
    }
    api->config_end(&appIter);
    return appOptions;
}

std::vector<std::string> getListItemPath(rime_api_t *api, RimeConfig *config,
                                         const std::string &path) {
    std::vector<std::string> paths;
    RimeConfigIterator iter;
    if (api->config_begin_list(&iter, config, path.c_str())) {
        while (api->config_next(&iter)) {
            paths.push_back(iter.path);
        }
    }
    return paths;
}

std::vector<std::string> getListItemString(rime_api_t *api, RimeConfig *config,
                                           const std::string &path) {
    std::vector<std::string> values;
    auto paths = getListItemPath(api, config, path);
    for (const auto &path : paths) {
        auto value = api->config_get_cstring(config, path.c_str());
        if (!value) {
            return {};
        }
        values.emplace_back(value);
    }
    return values;
}

rime_api_t *EnsureRimeApi() {
    auto *api = rime_get_api();
    if (!api) {
        throw std::runtime_error("Failed to get Rime API");
    }
    return api;
}

} // namespace

class IMAction : public Action {
public:
    IMAction(RimeEngine *engine) : engine_(engine) {}

    std::string shortText(InputContext *ic) const override {
        std::string result;
        auto state = engine_->state(ic);
        if (state) {
            state->getStatus([&result](const RimeStatus &status) {
                result = status.schema_id ? status.schema_id : "";
                if (status.is_disabled) {
                    result = "\xe2\x8c\x9b";
                } else if (status.is_ascii_mode) {
                    result = "A";
                } else if (status.schema_name && status.schema_name[0] != '.') {
                    result = status.schema_name;
                } else {
                    result = "中";
                }
            });
        } else {
            result = "\xe2\x8c\x9b";
        }
        return result;
    }

    std::string longText(InputContext *ic) const override {
        std::string result;
        auto state = engine_->state(ic);
        if (state) {
            state->getStatus([&result](const RimeStatus &status) {
                result = status.schema_name ? status.schema_name : "";
            });
        }
        return result;
    }

    std::string icon(InputContext *ic) const override {
        bool isDisabled = false;
        auto state = engine_->state(ic);
        if (state) {
            state->getStatus([&isDisabled](const RimeStatus &status) {
                isDisabled = status.is_disabled;
            });
        }
        if (isDisabled) {
            return "fcitx-rime-disabled";
        }
        return "fcitx-rime-im";
    }

private:
    RimeEngine *engine_;
};

class ToggleAction : public SimpleAction {
public:
    ToggleAction(RimeEngine *engine, std::string schema, std::string option,
                 std::string disabledText, std::string enabledText)
        : engine_(engine), option_(option), disabledText_(disabledText),
          enabledText_(enabledText) {
        connect<SimpleAction::Activated>([this](InputContext *ic) {
            auto state = engine_->state(ic);
            auto api = engine_->api();
            if (!state) {
                return;
            }
            // Do not send notification since user is explicitly select it.
            engine_->blockNotificationFor(30000);
            auto session = state->session();
            Bool oldValue = api->get_option(session, option_.c_str());
            api->set_option(session, option_.c_str(), !oldValue);
        });
        engine_->instance()->userInterfaceManager().registerAction(
            stringutils::concat("fcitx-rime-", schema, "-", option), this);
    }

    std::string shortText(InputContext *ic) const override {
        auto state = engine_->state(ic);
        auto api = engine_->api();
        if (!state) {
            return "";
        }
        auto session = state->session();
        if (api->get_option(session, option_.c_str())) {
            return stringutils::concat(enabledText_, " → ", disabledText_);
        }
        return stringutils::concat(disabledText_, " → ", enabledText_);
    }

    std::string icon(InputContext *) const override { return ""; }

private:
    RimeEngine *engine_;
    std::string option_;
    std::string disabledText_;
    std::string enabledText_;
};

class SelectAction : public Action {
public:
    SelectAction(RimeEngine *engine, std::string schema,
                 std::vector<std::string> options,
                 std::vector<std::string> texts)
        : engine_(engine), options_(options), texts_(texts) {
        for (size_t i = 0; i < options.size(); ++i) {
            actions_.emplace_back();
            actions_.back().setShortText(texts_[i]);
            actions_.back().connect<SimpleAction::Activated>(
                [this, i](InputContext *ic) {
                    auto state = engine_->state(ic);
                    auto api = engine_->api();
                    if (!state) {
                        return;
                    }
                    auto session = state->session();
                    for (size_t j = 0; j < options_.size(); ++j) {
                        api->set_option(session, options_[j].c_str(), i == j);
                    }
                });
            engine_->instance()->userInterfaceManager().registerAction(
                stringutils::concat("fcitx-rime-", schema, "-", options_[i]),
                &actions_.back());
            menu_.addAction(&actions_.back());
        }
        setMenu(&menu_);
        engine_->instance()->userInterfaceManager().registerAction(
            stringutils::concat("fcitx-rime-", schema, "-select-", options[0]),
            this);
    }

    std::string shortText(InputContext *ic) const override {
        auto state = engine_->state(ic);
        auto api = engine_->api();
        if (!state) {
            return "";
        }
        auto session = state->session();
        for (size_t i = 0; i < options_.size(); ++i) {
            if (api->get_option(session, options_[i].c_str())) {
                return texts_[i];
            }
        }
        return "";
    }

    std::string icon(InputContext *) const override { return ""; }

private:
    RimeEngine *engine_;
    std::vector<std::string> options_;
    std::vector<std::string> texts_;
    std::list<SimpleAction> actions_;
    Menu menu_;
};

RimeEngine::RimeEngine(Instance *instance)
    : instance_(instance), api_(EnsureRimeApi()),
      factory_([this](InputContext &ic) { return new RimeState(this, ic); }),
      sessionPool_(this, instance_->globalConfig().shareInputState()) {
#ifdef __ANDROID__
    const auto &sp = fcitx::StandardPath::global();
    std::string defaultYaml =
        sp.locate(fcitx::StandardPath::Type::Data, "rime-data/default.yaml");
    if (defaultYaml.empty()) {
        throw std::runtime_error("Fail to locate shared data directory");
    }
    sharedDataDir_ = fcitx::fs::dirName(defaultYaml);
#else
    sharedDataDir_ = RIME_DATA_DIR;
#endif
    imAction_ = std::make_unique<IMAction>(this);
    instance_->userInterfaceManager().registerAction("fcitx-rime-im",
                                                     imAction_.get());
    imAction_->setMenu(&schemaMenu_);
    eventDispatcher_.attach(&instance_->eventLoop());
    separatorAction_.setSeparator(true);
    instance_->userInterfaceManager().registerAction("fcitx-rime-separator",
                                                     &separatorAction_);
    deployAction_.setIcon("fcitx-rime-deploy");
    deployAction_.setShortText(_("Deploy"));
    deployAction_.connect<SimpleAction::Activated>([this](InputContext *ic) {
        deploy();
        auto state = this->state(ic);
        if (state && ic->hasFocus()) {
            state->updateUI(ic, false);
        }
    });
    instance_->userInterfaceManager().registerAction("fcitx-rime-deploy",
                                                     &deployAction_);

    syncAction_.setIcon("fcitx-rime-sync");
    syncAction_.setShortText(_("Synchronize"));

    syncAction_.connect<SimpleAction::Activated>([this](InputContext *ic) {
        sync();
        auto state = this->state(ic);
        if (state && ic->hasFocus()) {
            state->updateUI(ic, false);
        }
    });
    instance_->userInterfaceManager().registerAction("fcitx-rime-sync",
                                                     &syncAction_);
    schemaMenu_.addAction(&separatorAction_);
    schemaMenu_.addAction(&deployAction_);
    schemaMenu_.addAction(&syncAction_);
    globalConfigReloadHandle_ = instance_->watchEvent(
        EventType::GlobalConfigReloaded, EventWatcherPhase::Default,
        [this](Event &) {
            releaseAllSession();
            sessionPool_.setPropertyPropagatePolicy(
                instance_->globalConfig().shareInputState());
        });
    reloadConfig();
}

RimeEngine::~RimeEngine() {
    factory_.unregister();
    try {
        api_->finalize();
    } catch (const std::exception &e) {
        RIME_ERROR() << e.what();
    }
}

void RimeEngine::rimeStart(bool fullcheck) {
    RIME_DEBUG() << "Rime Start (fullcheck: " << fullcheck << ")";

    auto userDir = stringutils::joinPath(
        StandardPath::global().userDirectory(StandardPath::Type::PkgData),
        "rime");
    RIME_DEBUG() << "Rime data directory: " << userDir;
    if (!fs::makePath(userDir)) {
        if (!fs::isdir(userDir)) {
            RIME_ERROR() << "Failed to create user directory: " << userDir;
        }
    }

    RIME_STRUCT(RimeTraits, fcitx_rime_traits);
    fcitx_rime_traits.shared_data_dir = sharedDataDir_.c_str();
    fcitx_rime_traits.app_name = "rime.fcitx-rime";
    fcitx_rime_traits.user_data_dir = userDir.c_str();
    fcitx_rime_traits.distribution_name = "Rime";
    fcitx_rime_traits.distribution_code_name = "fcitx-rime";
    fcitx_rime_traits.distribution_version = FCITX_RIME_VERSION;
#ifndef FCITX_RIME_NO_LOG_LEVEL
    switch (rime().logLevel()) {
    case NoLog:
        fcitx_rime_traits.min_log_level = 4;
        break;
    case Fatal:
        fcitx_rime_traits.min_log_level = 3;
        break;
    case Error:
    case Warn:
    case Info:
        fcitx_rime_traits.min_log_level = 2;
        break;
    case Debug:
    default:
        // Rime info is too noisy.
        fcitx_rime_traits.min_log_level = 0;
        break;
    }
#endif

#ifdef FCITX_RIME_LOAD_PLUGIN
    std::vector<const char *> modules;
    // When it is not test, rime will load the default set.
    RIME_DEBUG() << "Modules: " << *config_.modules;
    if (!config_.modules->empty()) {
        modules.push_back("default");
        for (const std::string &module : *config_.modules) {
            modules.push_back(module.data());
        }
        modules.push_back(nullptr);
        fcitx_rime_traits.modules = modules.data();
    } else {
        fcitx_rime_traits.modules = nullptr;
    }
#else
    fcitx_rime_traits.modules = nullptr;
#endif

    if (firstRun_) {
        api_->setup(&fcitx_rime_traits);
        firstRun_ = false;
    }
    api_->initialize(&fcitx_rime_traits);
    api_->set_notification_handler(&rimeNotificationHandler, this);
    api_->start_maintenance(fullcheck);

    if (!api_->is_maintenance_mode()) {
        updateAppOptions();
    }
}

void RimeEngine::updateAppOptions() {
    appOptions_.clear();
    RimeConfig config = {NULL};
    if (api_->config_open("fcitx5", &config)) {
        appOptions_ = parseAppOptions(api_, &config);
        api_->config_close(&config);
    }
    RIME_DEBUG() << "App options are " << appOptions_;
    releaseAllSession();
}

void RimeEngine::reloadConfig() {
    readAsIni(config_, "conf/rime.conf");
    updateConfig();
}

void RimeEngine::setSubConfig(const std::string &path, const RawConfig &) {
    if (path == "deploy") {
        deploy();
    } else if (path == "sync") {
        sync();
    }
}

void RimeEngine::updateConfig() {
    RIME_DEBUG() << "Rime UpdateConfig";
    factory_.unregister();
    try {
        api_->finalize();
    } catch (const std::exception &e) {
        RIME_ERROR() << e.what();
    }

#ifdef FCITX_RIME_LOAD_PLUGIN
    std::vector<std::string> plugins;
    if (*config_.autoloadPlugins) {
        auto closedir0 = [](DIR *dir) {
            if (dir) {
                closedir(dir);
            }
        };

        const char *libdir = StandardPath::fcitxPath("libdir");
        std::unique_ptr<DIR, void (*)(DIR *)> scopedDir{opendir(libdir),
                                                        closedir0};
        if (scopedDir) {
            auto dir = scopedDir.get();
            struct dirent *drt;
            while ((drt = readdir(dir)) != nullptr) {
                if (strcmp(drt->d_name, ".") == 0 ||
                    strcmp(drt->d_name, "..") == 0) {
                    continue;
                }

                auto name = drt->d_name;
                if (stringutils::startsWith(name, "librime-") &&
                    stringutils::endsWith(name, ".so")) {
                    plugins.push_back(
                        stringutils::joinPath(libdir, std::move(name)));
                }
            }
        }
    } else {
        plugins = *config_.plugins;
    }

    for (const std::string &plugin : plugins) {
        if (pluginPool_.count(plugin)) {
            continue;
        }
        pluginPool_.emplace(plugin, Library(plugin));
        pluginPool_[plugin].load({LibraryLoadHint::ExportExternalSymbolsHint});
        RIME_DEBUG() << "Trying to load rime plugin: " << plugin;
        if (!pluginPool_[plugin].loaded()) {
            RIME_ERROR() << "Failed to load plugin: " << plugin
                         << " error: " << pluginPool_[plugin].error();
            pluginPool_.erase(plugin);
        }
    }
#endif

    rimeStart(false);
    instance_->inputContextManager().registerProperty("rimeState", &factory_);
    updateSchemaMenu();
}

void RimeEngine::refreshStatusArea(InputContext &ic) {
    // prevent modifying status area owned by other ime
    // e.g. keyboard-us when typing password
    if (instance_->inputMethod(&ic) != "rime") {
        return;
    }
    auto &statusArea = ic.statusArea();
    statusArea.clearGroup(StatusGroup::InputMethod);
    statusArea.addAction(StatusGroup::InputMethod, imAction_.get());

    auto rimeState = state(&ic);
    std::string currentSchema;
    if (!rimeState) {
        return;
    }
    rimeState->getStatus([&currentSchema](const RimeStatus &status) {
        currentSchema = status.schema_id ? status.schema_id : "";
    });
    if (currentSchema.empty()) {
        return;
    }

    if (auto iter = optionActions_.find(currentSchema);
        iter != optionActions_.end()) {
        for (const auto &action : iter->second) {
            statusArea.addAction(StatusGroup::InputMethod, action.get());
        }
    }
}

void RimeEngine::refreshStatusArea(RimeSessionId session) {
    instance_->inputContextManager().foreachFocused(
        [this, session](InputContext *ic) {
            if (auto state = this->state(ic)) {
                // After a deployment, param is 0, refresh all
                if (!session || state->session(false) == session) {
                    refreshStatusArea(*ic);
                }
            }
            return true;
        });
}

void RimeEngine::updateStatusArea(RimeSessionId session) {
    instance_->inputContextManager().foreachFocused(
        [this, session](InputContext *ic) {
            if (instance_->inputMethod(ic) != "rime") {
                return true;
            }
            if (auto state = this->state(ic)) {
                // After a deployment, param is 0, refresh all
                if (!session || state->session(false) == session) {
                    // Re-read new option values.
                    ic->updateUserInterface(UserInterfaceComponent::StatusArea);
                }
            }
            return true;
        });
}

void RimeEngine::activate(const InputMethodEntry &, InputContextEvent &event) {
    auto ic = event.inputContext();
    refreshStatusArea(*ic);
}

void RimeEngine::deactivate(const InputMethodEntry &entry,
                            InputContextEvent &event) {
    if (event.type() == EventType::InputContextSwitchInputMethod &&
        *config_.commitWhenDeactivate) {
        auto inputContext = event.inputContext();
        auto state = inputContext->propertyFor(&factory_);
        state->commitPreedit(inputContext);
    }
    reset(entry, event);
}

void RimeEngine::keyEvent(const InputMethodEntry &entry, KeyEvent &event) {
    FCITX_UNUSED(entry);
    RIME_DEBUG() << "Rime receive key: " << event.rawKey() << " "
                 << event.isRelease();
    auto inputContext = event.inputContext();
    auto state = inputContext->propertyFor(&factory_);
    state->keyEvent(event);
}

void RimeEngine::reset(const InputMethodEntry &, InputContextEvent &event) {
    auto inputContext = event.inputContext();

    auto state = inputContext->propertyFor(&factory_);
    state->clear();
    inputContext->inputPanel().reset();
    inputContext->updatePreedit();
    inputContext->updateUserInterface(UserInterfaceComponent::InputPanel);
}

void RimeEngine::blockNotificationFor(uint64_t usec) {
    blockNotificationBefore_ = now(CLOCK_MONOTONIC) + usec;
}

void RimeEngine::save() {
    // Block notification for 5 sec.
    blockNotificationFor(5000000);
    sync();
}

void RimeEngine::rimeNotificationHandler(void *context, RimeSessionId session,
                                         const char *messageType,
                                         const char *messageValue) {
    RIME_DEBUG() << "Notification: " << session << " " << messageType << " "
                 << messageValue;
    RimeEngine *that = static_cast<RimeEngine *>(context);
    that->eventDispatcher_.schedule(
        [that, session, messageType = std::string(messageType),
         messageValue = std::string(messageValue)]() {
            that->notify(session, messageType, messageValue);
        });
}

void RimeEngine::notify(RimeSessionId session, const std::string &messageType,
                        const std::string &messageValue) {
    const char *message = nullptr;
    const char *icon = "";
    const char *tipId = "";
    if (messageType == "deploy") {
        tipId = "fcitx-rime-deploy";
        icon = "fcitx-rime-deploy";
        if (messageValue == "start") {
            message = _("Rime is under maintenance. It may take a few "
                        "seconds. Please wait until it is finished...");
        } else if (messageValue == "success") {
            message = _("Rime is ready.");
            updateSchemaMenu();
            refreshStatusArea(0);
            if (!api_->is_maintenance_mode()) {
                api_->deploy_config_file("fcitx5.yaml", "config_version");
                updateAppOptions();
            }
        } else if (messageValue == "failure") {
            message = _("Rime has encountered an error. "
                        "See /tmp/rime.fcitx.ERROR for details.");
        }
    } else if (messageType == "option") {
        icon = "fcitx-rime";
        if (messageValue == "!full_shape") {
            tipId = "fcitx-rime-full-shape";
            message = _("Half Shape is enabled.");
        } else if (messageValue == "full_shape") {
            tipId = "fcitx-rime-full-shape";
            message = _("Full Shape is enabled.");
        } else if (messageValue == "!ascii_punct") {
            tipId = "fcitx-rime-ascii-punct";
            message = _("Punctuation conversion is enabled.");
        } else if (messageValue == "ascii_punct") {
            tipId = "fcitx-rime-ascii-punct";
            message = _("Punctuation conversion is disabled.");
        } else if (messageValue == "!simplification") {
            tipId = "fcitx-rime-simplification";
            message = _("Traditional Chinese is enabled.");
        } else if (messageValue == "simplification") {
            tipId = "fcitx-rime-simplification";
            message = _("Simplified Chinese is enabled.");
        }
        updateStatusArea(session);
    } else if (messageType == "schema") {
        // Schema is changed either via status area or shortcut
        refreshStatusArea(session);
    }

    auto notifications = this->notifications();
    if (message && notifications &&
        now(CLOCK_MONOTONIC) > blockNotificationBefore_) {
        notifications->call<INotifications::showTip>(tipId, _("Rime"), icon,
                                                     _("Rime"), message, -1);
    }
}

RimeState *RimeEngine::state(InputContext *ic) {
    if (!factory_.registered()) {
        return nullptr;
    }
    return ic->propertyFor(&factory_);
}

std::string RimeEngine::subMode(const InputMethodEntry &, InputContext &ic) {
    if (auto rimeState = state(&ic)) {
        return rimeState->subMode();
    }
    return "";
}

std::string RimeEngine::subModeLabelImpl(const InputMethodEntry &,
                                         InputContext &ic) {
    if (auto rimeState = state(&ic)) {
        return rimeState->subModeLabel();
    }
    return "";
}

std::string RimeEngine::subModeIconImpl(const InputMethodEntry &,
                                        InputContext &ic) {
    std::string result = "fcitx-rime";
    if (!factory_.registered()) {
        return result;
    }
    auto state = this->state(&ic);
    if (state) {
        state->getStatus([&result](const RimeStatus &status) {
            if (status.is_disabled) {
                result = "fcitx-rime-disable";
            } else if (status.is_ascii_mode) {
                result = "fcitx-rime-latin";
            } else {
                result = "fcitx-rime";
            }
        });
    }
    return result;
}

void RimeEngine::releaseAllSession() {
    instance_->inputContextManager().foreach([this](InputContext *ic) {
        if (auto state = this->state(ic)) {
            state->release();
        }
        return true;
    });
}

void RimeEngine::deploy() {
    RIME_DEBUG() << "Rime Deploy";
    releaseAllSession();
    api_->finalize();
    rimeStart(true);
}

void RimeEngine::sync() {
    RIME_DEBUG() << "Rime Sync user data";
    api_->sync_user_data();
    releaseAllSession();
}

void RimeEngine::updateActionsForSchema(const std::string &schema) {
    RimeConfig config{};

    if (!api_->schema_open(schema.c_str(), &config)) {
        return;
    }
    auto switchPaths = getListItemPath(api_, &config, "switches");
    for (const auto &switchPath : switchPaths) {
        auto labels = getListItemString(api_, &config, switchPath + "/states");
        if (labels.size() <= 1) {
            continue;
        }
        auto namePath = switchPath + "/name";
        auto name = api_->config_get_cstring(&config, namePath.c_str());
        if (name) {
            if (labels.size() != 2) {
                continue;
            }
            std::string optionName = name;
            if (optionName == RIME_ASCII_MODE) {
                // imAction_ has latin mode that does the same
                continue;
            }

            optionActions_[schema].emplace_back(std::make_unique<ToggleAction>(
                this, schema, optionName, labels[0], labels[1]));
        } else {
            auto options =
                getListItemString(api_, &config, switchPath + "/options");
            if (labels.size() != options.size()) {
                continue;
            }
            optionActions_[schema].emplace_back(
                std::make_unique<SelectAction>(this, schema, options, labels));
        }
    }
    api_->config_close(&config);
}

void RimeEngine::updateSchemaMenu() {
    schemActions_.clear();
    optionActions_.clear();
    RimeSchemaList list;
    list.size = 0;
    if (api_->get_schema_list(&list)) {
        schemActions_.emplace_back();

        schemActions_.back().setShortText(_("Latin Mode"));
        schemActions_.back().connect<SimpleAction::Activated>(
            [this](InputContext *ic) {
                auto state = ic->propertyFor(&factory_);
                state->setLatinMode(true);
                imAction_->update(ic);
            });
        instance_->userInterfaceManager().registerAction(&schemActions_.back());
        schemaMenu_.insertAction(&separatorAction_, &schemActions_.back());
        for (size_t i = 0; i < list.size; i++) {
            schemActions_.emplace_back();
            std::string schemaId = list.list[i].schema_id;
            auto &schemaAction = schemActions_.back();
            schemaAction.setShortText(list.list[i].name);
            schemaAction.connect<SimpleAction::Activated>(
                [this, schemaId](InputContext *ic) {
                    auto state = ic->propertyFor(&factory_);
                    blockNotificationFor(30000);
                    state->selectSchema(schemaId);
                    imAction_->update(ic);
                });
            instance_->userInterfaceManager().registerAction(&schemaAction);
            schemaMenu_.insertAction(&separatorAction_, &schemaAction);
            updateActionsForSchema(schemaId);
        }
        api_->free_schema_list(&list);
    }
}

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::RimeEngineFactory)
