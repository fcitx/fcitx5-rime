/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "rimeengine.h"
#include "notifications_public.h"
#include "rimestate.h"
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
#include <fcitx/userinterfacemanager.h>
#include <rime_api.h>

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
} // namespace

class IMAction : public Action {
public:
    IMAction(RimeEngine *engine) : engine_(engine) {}

    std::string shortText(InputContext *ic) const override {
        auto state = engine_->state(ic);
        RIME_STRUCT(RimeStatus, status);
        std::string result;
        if (state->getStatus(&status)) {
            if (status.is_disabled) {
                result = "\xe2\x8c\x9b";
            } else if (status.is_ascii_mode) {
                result = "A";
            } else if (status.schema_name && status.schema_name[0] != '.') {
                result = status.schema_name;
            } else {
                result = "中";
            }
            engine_->api()->free_status(&status);
        } else {
            result = "\xe2\x8c\x9b";
        }
        return result;
    }

    std::string longText(InputContext *ic) const override {
        auto state = engine_->state(ic);
        std::string result;
        RIME_STRUCT(RimeStatus, status);
        if (state->getStatus(&status)) {
            if (status.schema_name) {
                result = status.schema_name;
            }
            engine_->api()->free_status(&status);
        }
        return result;
    }

    std::string icon(InputContext *ic) const override {
        auto state = engine_->state(ic);
        RIME_STRUCT(RimeStatus, status);
        if (state->getStatus(&status)) {
            if (status.is_disabled) {
                return "fcitx-rime-disabled";
            }
        }
        return "fcitx-rime-im";
    }

private:
    RimeEngine *engine_;
};

RimeEngine::RimeEngine(Instance *instance)
    : instance_(instance), api_(rime_get_api()),
      factory_([this](InputContext &ic) { return new RimeState(this, ic); }) {
    imAction_ = std::make_unique<IMAction>(this);
    instance_->userInterfaceManager().registerAction("fcitx-rime-im",
                                                     imAction_.get());
    imAction_->setMenu(&schemaMenu_);
    eventDispatcher_.attach(&instance_->eventLoop());
    deployAction_.setIcon("fcitx-rime-deploy");
    deployAction_.setShortText(_("Deploy"));
    deployAction_.connect<SimpleAction::Activated>([this](InputContext *ic) {
        deploy();
        auto state = this->state(ic);
        if (ic->hasFocus()) {
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
        if (ic->hasFocus()) {
            state->updateUI(ic, false);
        }
    });
    instance_->userInterfaceManager().registerAction("fcitx-rime-sync",
                                                     &syncAction_);
    reloadConfig();
}

RimeEngine::~RimeEngine() {
    factory_.unregister();
    try {
        if (api_) {
            api_->finalize();
        }
    } catch (const std::exception &e) {
        RIME_ERROR() << e.what();
    }
}

void RimeEngine::rimeStart(bool fullcheck) {
    if (!api_) {
        return;
    }

    RIME_DEBUG() << "Rime Start (fullcheck: " << fullcheck << ")";

    auto userDir = stringutils::joinPath(
        StandardPath::global().userDirectory(StandardPath::Type::PkgData),
        "rime");
    if (!fs::makePath(userDir)) {
        if (!fs::isdir(userDir)) {
            RIME_ERROR() << "Failed to create user directory: " << userDir;
        }
    }
    const char *sharedDataDir = RIME_DATA_DIR;

    RIME_STRUCT(RimeTraits, fcitx_rime_traits);
    fcitx_rime_traits.shared_data_dir = sharedDataDir;
    fcitx_rime_traits.app_name = "rime.fcitx-rime";
    fcitx_rime_traits.user_data_dir = userDir.c_str();
    fcitx_rime_traits.distribution_name = "Rime";
    fcitx_rime_traits.distribution_code_name = "fcitx-rime";
    fcitx_rime_traits.distribution_version = FCITX_RIME_VERSION;

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
    if (api_->start_maintenance(fullcheck)) {
        api_->deploy_config_file("fcitx5.yaml", "config_version");
    }

    appOptions_.clear();
    RimeConfig config = {NULL};
    if (api_->config_open("fcitx5", &config)) {
        appOptions_ = parseAppOptions(api_, &config);
        api_->config_close(&config);
    }
    RIME_DEBUG() << "App options are " << appOptions_;
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
    if (api_) {
        try {
            api_->finalize();
        } catch (const std::exception &e) {
            RIME_ERROR() << e.what();
        }
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
void RimeEngine::activate(const InputMethodEntry &, InputContextEvent &event) {
    event.inputContext()->statusArea().addAction(StatusGroup::InputMethod,
                                                 imAction_.get());
    event.inputContext()->statusArea().addAction(StatusGroup::InputMethod,
                                                 &deployAction_);
    event.inputContext()->statusArea().addAction(StatusGroup::InputMethod,
                                                 &syncAction_);
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

void RimeEngine::save() {}

void RimeEngine::rimeNotificationHandler(void *context, RimeSessionId session,
                                         const char *messageType,
                                         const char *messageValue) {
    RIME_DEBUG() << "Notification: " << session << " " << messageType << " "
                 << messageValue;
    RimeEngine *that = static_cast<RimeEngine *>(context);
    that->eventDispatcher_.schedule(
        [that, messageType = std::string(messageType),
         messageValue = std::string(messageValue)]() {
            that->notify(messageType, messageValue);
        });
}

void RimeEngine::notify(const std::string &messageType,
                        const std::string &messageValue) {
    const char *message = nullptr;
    if (messageType == "deploy") {
        if (messageValue == "start") {
            message = _("Rime is under maintenance. It may take a few "
                        "seconds. Please wait until it is finished...");
        } else if (messageValue == "success") {
            message = _("Rime is ready.");
            updateSchemaMenu();
        } else if (messageValue == "failure") {
            message = _("Rime has encountered an error. "
                        "See /tmp/rime.fcitx.ERROR for details.");
        }
    }

    auto notifications = this->notifications();
    if (message && notifications) {
        notifications->call<INotifications::showTip>(
            "fcitx-rime-deploy", _("Rime"), "fcitx-rime-deploy", _("Rime"),
            message, -1);
    }
    timeEvent_ = instance_->eventLoop().addTimeEvent(
        CLOCK_MONOTONIC, now(CLOCK_MONOTONIC) + 1000000, 0,
        [this](EventSourceTime *, uint64_t) {
            if (auto *ic = instance_->lastFocusedInputContext()) {
                imAction_->update(ic);
                ic->updateUserInterface(UserInterfaceComponent::StatusArea);
            }
            return true;
        });
}

RimeState *RimeEngine::state(InputContext *ic) {
    return ic->propertyFor(&factory_);
}

std::string RimeEngine::subMode(const InputMethodEntry &, InputContext &ic) {
    auto rimeState = state(&ic);
    return rimeState->subMode();
}

std::string RimeEngine::subModeLabelImpl(const InputMethodEntry &,
                                         InputContext &ic) {
    auto rimeState = state(&ic);
    return rimeState->subModeLabel();
}

std::string RimeEngine::subModeIconImpl(const InputMethodEntry &,
                                        InputContext &ic) {
    std::string result = "fcitx-rime";
    if (!api_ || !factory_.registered()) {
        return result;
    }
    auto state = this->state(&ic);
    RIME_STRUCT(RimeStatus, status);
    if (state->getStatus(&status)) {
        if (status.is_disabled) {
            result = "fcitx-rime-disable";
        } else if (status.is_ascii_mode) {
            result = "fcitx-rime-latin";
        } else {
            result = "fcitx-rime";
        }
        api_->free_status(&status);
    }
    return result;
}

void RimeEngine::deploy() {
    RIME_DEBUG() << "Rime Deploy";
    instance_->inputContextManager().foreach([this](InputContext *ic) {
        auto state = this->state(ic);
        state->release();
        return true;
    });
    api_->finalize();
    rimeStart(true);
}

void RimeEngine::sync() { api_->sync_user_data(); }

void RimeEngine::updateSchemaMenu() {
    if (!api_) {
        return;
    }

    schemActions_.clear();
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
        schemaMenu_.addAction(&schemActions_.back());
        for (size_t i = 0; i < list.size; i++) {
            schemActions_.emplace_back();
            std::string schemaId = list.list[i].schema_id;
            auto &schemaAction = schemActions_.back();
            schemaAction.setShortText(list.list[i].name);
            schemaAction.connect<SimpleAction::Activated>(
                [this, schemaId](InputContext *ic) {
                    auto state = ic->propertyFor(&factory_);
                    state->selectSchema(schemaId);
                    imAction_->update(ic);
                });
            instance_->userInterfaceManager().registerAction(&schemaAction);
            schemaMenu_.addAction(&schemaAction);
        }
        api_->free_schema_list(&list);
    }
}

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::RimeEngineFactory)
