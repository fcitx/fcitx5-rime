/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "rimeengine.h"
#include "notifications_public.h"
#include "rimeaction.h"
#include "rimestate.h"
#include <cstdint>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <exception>
#include <fcitx-config/iniparser.h>
#include <fcitx-config/rawconfig.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/eventloopinterface.h>
#include <fcitx-utils/fs.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/macros.h>
#include <fcitx-utils/misc.h>
#include <fcitx-utils/standardpaths.h>
#include <fcitx-utils/stringutils.h>
#include <fcitx/action.h>
#include <fcitx/addoninstance.h>
#include <fcitx/candidatelist.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodentry.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/menu.h>
#include <fcitx/statusarea.h>
#include <fcitx/userinterface.h>
#include <fcitx/userinterfacemanager.h>
#include <list>
#include <memory>
#include <rime_api.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

FCITX_DEFINE_LOG_CATEGORY(rime_log, "rime");

namespace fcitx::rime {

namespace {

// Allow notification for 60secs.
constexpr uint64_t NotificationTimeout = 60000000;

std::unordered_map<std::string, std::unordered_map<std::string, bool>>
parseAppOptions(rime_api_t *api, RimeConfig *config) {
    std::unordered_map<std::string, std::unordered_map<std::string, bool>>
        appOptions;
    RimeConfigIterator appIter;
    RimeConfigIterator optionIter;
    if (api->config_begin_map(&appIter, config, "app_options")) {
        while (api->config_next(&appIter)) {
            auto &options = appOptions[appIter.key];
            if (api->config_begin_map(&optionIter, config, appIter.path)) {
                while (api->config_next(&optionIter)) {
                    Bool value = False;
                    if (api->config_get_bool(config, optionIter.path, &value)) {
                        options[optionIter.key] = !!value;
                    }
                }
                api->config_end(&optionIter);
            }
        }
        api->config_end(&appIter);
    }
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
        api->config_end(&iter);
    }
    return paths;
}

std::vector<std::string> getListItemString(rime_api_t *api, RimeConfig *config,
                                           const std::string &path) {
    std::vector<std::string> values;
    auto paths = getListItemPath(api, config, path);
    for (const auto &path : paths) {
        const auto *value = api->config_get_cstring(config, path.c_str());
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
        auto *state = engine_->state(ic);
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
        auto *state = engine_->state(ic);
        if (state) {
            state->getStatus([&result](const RimeStatus &status) {
                result = status.schema_name ? status.schema_name : "";
            });
        }
        return result;
    }

    std::string icon(InputContext *ic) const override {
        bool isDisabled = false;
        auto *state = engine_->state(ic);
        if (state) {
            state->getStatus([&isDisabled](const RimeStatus &status) {
                isDisabled = status.is_disabled;
            });
        }
        if (isDisabled) {
            return "fcitx_rime_disabled";
        }
        return "fcitx_rime_im";
    }

private:
    RimeEngine *engine_;
};

bool RimeEngine::firstRun_ = true;

RimeEngine::RimeEngine(Instance *instance)
    : instance_(instance), api_(EnsureRimeApi()),
      factory_([this](InputContext &ic) { return new RimeState(this, ic); }),
      sessionPool_(this, getSharedStatePolicy()) {
    if constexpr (isAndroid() || isApple()) {
        const auto &sp = StandardPaths::global();
        std::string defaultYaml =
            sp.locate(StandardPathsType::Data, "rime-data/default.yaml");
        if (defaultYaml.empty()) {
            throw std::runtime_error("Fail to locate shared data directory");
        }
        sharedDataDir_ = fcitx::fs::dirName(defaultYaml);
    } else {
        sharedDataDir_ = RIME_DATA_DIR;
    }
    imAction_ = std::make_unique<IMAction>(this);
    instance_->userInterfaceManager().registerAction("fcitx-rime-im",
                                                     imAction_.get());
    imAction_->setMenu(&schemaMenu_);
    eventDispatcher_.attach(&instance_->eventLoop());
    separatorAction_.setSeparator(true);
    instance_->userInterfaceManager().registerAction("fcitx-rime-separator",
                                                     &separatorAction_);
    deployAction_.setIcon("fcitx_rime_deploy");
    deployAction_.setShortText(_("Deploy"));
    deployAction_.connect<SimpleAction::Activated>([this](InputContext *ic) {
        deploy();
        auto *state = this->state(ic);
        if (state && ic->hasFocus()) {
            state->updateUI(ic, false);
        }
    });
    instance_->userInterfaceManager().registerAction("fcitx-rime-deploy",
                                                     &deployAction_);

    syncAction_.setIcon("fcitx_rime_sync");
    syncAction_.setShortText(_("Synchronize"));

    syncAction_.connect<SimpleAction::Activated>([this](InputContext *ic) {
        sync(/*userTriggered=*/true);
        auto *state = this->state(ic);
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
        [this](Event &) { refreshSessionPoolPolicy(); });

    allowNotification("failure");
    reloadConfig();
    constructed_ = true;
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

    auto userDir =
        StandardPaths::global().userDirectory(StandardPathsType::PkgData) /
        "rime";
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
    // make librime only log to stderr
    // https://github.com/rime/librime/commit/6d1b9b65de4e7784a68a17d10a3e5c900e4fd511
    fcitx_rime_traits.log_dir = "";
    switch (rime_log().logLevel()) {
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

    fcitx_rime_traits.modules = nullptr;

    if (firstRun_) {
        api_->setup(&fcitx_rime_traits);
        firstRun_ = false;
    }
    api_->initialize(&fcitx_rime_traits);
    api_->set_notification_handler(&rimeNotificationHandler, this);
    api_->start_maintenance(fullcheck);

    if (!api_->is_maintenance_mode()) {
        updateAppOptions();
    } else {
        needRefreshAppOption_ = true;
    }
}

void RimeEngine::updateAppOptions() {
    appOptions_.clear();
    RimeConfig config = {nullptr};
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

void RimeEngine::setSubConfig(const std::string &path,
                              const RawConfig & /*unused*/) {
    if (path == "deploy") {
        deploy();
    } else if (path == "sync") {
        sync(/*userTriggered=*/true);
    }
}

void RimeEngine::updateConfig() {
    RIME_DEBUG() << "Rime UpdateConfig";
    if (constructed_ && factory_.registered()) {
        releaseAllSession(true);
    }
    try {
        api_->finalize();
    } catch (const std::exception &e) {
        RIME_ERROR() << e.what();
    }

    rimeStart(false);
    instance_->inputContextManager().registerProperty("rimeState", &factory_);
    updateSchemaMenu();
    refreshSessionPoolPolicy();

    deployAction_.setHotkey(config_.deploy.value());
    syncAction_.setHotkey(config_.synchronize.value());

    if (constructed_) {
        refreshStatusArea(0);
    }
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

    auto *rimeState = state(&ic);
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
            if (auto *state = this->state(ic)) {
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
            if (auto *state = this->state(ic)) {
                // After a deployment, param is 0, refresh all
                if (!session || state->session(false) == session) {
                    // Re-read new option values.
                    ic->updateUserInterface(UserInterfaceComponent::StatusArea);
                }
            }
            return true;
        });
}

void RimeEngine::activate(const InputMethodEntry & /*entry*/,
                          InputContextEvent &event) {
    auto *ic = event.inputContext();
    refreshStatusArea(*ic);
    if (auto *state = this->state(ic)) {
        state->activate();
    }
}

void RimeEngine::deactivate(const InputMethodEntry &entry,
                            InputContextEvent &event) {
    if (event.type() == EventType::InputContextSwitchInputMethod) {
        auto *inputContext = event.inputContext();
        auto *state = this->state(inputContext);
        switch (*config_.switchInputMethodBehavior) {
        case SwitchInputMethodBehavior::Clear:
            break;
        case SwitchInputMethodBehavior::CommitRawInput:
            state->commitInput(inputContext);
            break;
        case SwitchInputMethodBehavior::CommitComposingText:
            state->commitComposing(inputContext);
            break;
        case SwitchInputMethodBehavior::CommitCommitPreview:
            state->commitPreedit(inputContext);
            break;
        }
    }
    reset(entry, event);
}

void RimeEngine::keyEvent(const InputMethodEntry &entry, KeyEvent &event) {
    FCITX_UNUSED(entry);
    RIME_DEBUG() << "Rime receive key: " << event.rawKey() << " "
                 << event.isRelease();
    auto *inputContext = event.inputContext();
    if (!event.isRelease()) {
        if (event.key().checkKeyList(*config_.deploy)) {
            deploy();
            event.filterAndAccept();
            return;
        }
        if (event.key().checkKeyList(*config_.synchronize)) {
            sync(/*userTriggered=*/true);
            event.filterAndAccept();
            return;
        }
    }
    auto *state = this->state(inputContext);
    currentKeyEventState_ = state;
    state->keyEvent(event);
    currentKeyEventState_ = nullptr;
}

void RimeEngine::reset(const InputMethodEntry & /*entry*/,
                       InputContextEvent &event) {
    auto *inputContext = event.inputContext();
    auto *state = this->state(inputContext);
    state->clear();
    instance_->resetCompose(inputContext);
    inputContext->inputPanel().reset();
    inputContext->updatePreedit();
    inputContext->updateUserInterface(UserInterfaceComponent::InputPanel);
}

void RimeEngine::allowNotification(std::string type) {
    allowNotificationUntil_ = now(CLOCK_MONOTONIC) + NotificationTimeout;
    allowNotificationType_ = std::move(type);
}

void RimeEngine::save() { sync(/*userTriggered=*/false); }

void RimeEngine::rimeNotificationHandler(void *context, RimeSessionId session,
                                         const char *messageType,
                                         const char *messageValue) {
    RIME_DEBUG() << "Notification: " << session << " " << messageType << " "
                 << messageValue;
    auto *that = static_cast<RimeEngine *>(context);
    if (that->mainThreadId_ == std::this_thread::get_id()) {
        that->notifyImmediately(session, messageType, messageValue);
    }
    that->eventDispatcher_.schedule(
        [that, session, messageType = std::string(messageType),
         messageValue = std::string(messageValue)]() {
            that->notify(session, messageType, messageValue);
        });
}

void RimeEngine::notifyImmediately(RimeSessionId session,
                                   std::string_view messageType,
                                   std::string_view messageValue) {
    if (messageType != "option") {
        return;
    }
    if (!currentKeyEventState_ ||
        currentKeyEventState_->session(false) != session) {
        return;
    }
    currentKeyEventState_->addChangedOption(messageValue);
}

void RimeEngine::notify(RimeSessionId session, const std::string &messageType,
                        const std::string &messageValue) {
    const char *message = nullptr;
    const char *icon = "";
    const char *tipId = "";
    const int timeout = 3000;
    bool blockMessage = false;
    if (messageType == "deploy") {
        tipId = "fcitx-rime-deploy";
        icon = "fcitx_rime_deploy";
        if (messageValue == "start") {
            message = _("Rime is under maintenance. It may take a few "
                        "seconds. Please wait until it is finished...");
        } else if (messageValue == "success") {
            message = _("Rime is ready.");
            if (!api_->is_maintenance_mode()) {
                if (needRefreshAppOption_) {
                    api_->deploy_config_file("fcitx5.yaml", "config_version");
                    updateAppOptions();
                    needRefreshAppOption_ = false;
                }
            }
            updateSchemaMenu();
            refreshStatusArea(0);
            blockMessage = true;
        } else if (messageValue == "failure") {
            needRefreshAppOption_ = false;
            message = _("Rime has encountered an error. "
                        "See log for details.");
            blockMessage = true;
        }
    } else if (messageType == "option") {
        updateStatusArea(session);
    } else if (messageType == "schema") {
        // Schema is changed either via status area or shortcut
        refreshStatusArea(session);
    }

    auto *notifications = this->notifications();
    const auto current = now(CLOCK_MONOTONIC);
    if (message && notifications && current > silenceNotificationUntil_ &&
        (current < allowNotificationUntil_ &&
         (allowNotificationType_.empty() ||
          messageType == allowNotificationType_))) {
        notifications->call<INotifications::showTip>(
            tipId, _("Rime"), icon, _("Rime"), message, timeout);
    }
    // Block message after error / success.
    if (blockMessage) {
        silenceNotificationUntil_ = current + 30000;
    }
}

RimeState *RimeEngine::state(InputContext *ic) {
    if (!factory_.registered()) {
        return nullptr;
    }
    return ic->propertyFor(&factory_);
}

std::string RimeEngine::subMode(const InputMethodEntry & /*entry*/,
                                InputContext &ic) {
    if (auto *rimeState = state(&ic)) {
        return rimeState->subMode();
    }
    return "";
}

std::string RimeEngine::subModeLabelImpl(const InputMethodEntry & /*unused*/,
                                         InputContext &ic) {
    if (auto *rimeState = state(&ic)) {
        return rimeState->subModeLabel();
    }
    return "";
}

std::string RimeEngine::subModeIconImpl(const InputMethodEntry & /*unused*/,
                                        InputContext &ic) {
    std::string result = "fcitx-rime";
    if (!factory_.registered()) {
        return result;
    }
    auto *state = this->state(&ic);
    if (state) {
        state->getStatus([&result](const RimeStatus &status) {
            if (status.is_disabled) {
                result = "fcitx_rime_disable";
            } else if (status.is_ascii_mode) {
                result = "fcitx_rime_latin";
            } else {
                result = "fcitx-rime";
            }
        });
    }
    return result;
}

void RimeEngine::releaseAllSession(bool snapshot) {
    instance_->inputContextManager().foreach([&](InputContext *ic) {
        if (auto *state = this->state(ic)) {
            if (snapshot) {
                state->snapshot();
            }
            state->release();
        }
        return true;
    });
}

void RimeEngine::deploy() {
    RIME_DEBUG() << "Rime Deploy";
    releaseAllSession(true);
    api_->finalize();
    allowNotification();
    rimeStart(true);
}

void RimeEngine::sync(bool userTriggered) {
    RIME_DEBUG() << "Rime Sync user data";
    releaseAllSession(true);
    if (userTriggered) {
        allowNotification();
    }
    api_->sync_user_data();
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
        const auto *name = api_->config_get_cstring(&config, namePath.c_str());
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
    schemas_.clear();
    schemActions_.clear();
    optionActions_.clear();
    RimeSchemaList list;
    list.size = 0;
    if (api_->get_schema_list(&list)) {
        schemActions_.emplace_back();

        schemActions_.back().setShortText(_("Latin Mode"));
        schemActions_.back().connect<SimpleAction::Activated>(
            [this](InputContext *ic) {
                auto *state = this->state(ic);
                state->toggleLatinMode();
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
                    auto *state = this->state(ic);
                    state->selectSchema(schemaId);
                    imAction_->update(ic);
                });
            instance_->userInterfaceManager().registerAction(&schemaAction);
            schemaMenu_.insertAction(&separatorAction_, &schemaAction);
            updateActionsForSchema(schemaId);
            schemas_.insert(schemaId);
        }
        api_->free_schema_list(&list);
    }
}

void RimeEngine::refreshSessionPoolPolicy() {
    auto newPolicy = getSharedStatePolicy();
    if (sessionPool_.propertyPropagatePolicy() != newPolicy) {
        releaseAllSession(constructed_);
        sessionPool_.setPropertyPropagatePolicy(newPolicy);
    }
}

PropertyPropagatePolicy RimeEngine::getSharedStatePolicy() {
    switch (*config_.sharedStatePolicy) {
    case SharedStatePolicy::All:
        return PropertyPropagatePolicy::All;
    case SharedStatePolicy::Program:
        return PropertyPropagatePolicy::Program;
    case SharedStatePolicy::No:
        return PropertyPropagatePolicy::No;
    case SharedStatePolicy::FollowGlobalConfig:
    default:
        return instance_->globalConfig().shareInputState();
    }
}

} // namespace fcitx::rime
