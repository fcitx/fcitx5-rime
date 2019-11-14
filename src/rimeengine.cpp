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

#include "rimeengine.h"
#include "notifications_public.h"
#include "rimestate.h"
#include <cstring>
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
        std::string result = "rime-disable";
        RIME_STRUCT(RimeStatus, status);
        if (state->getStatus(&status)) {
            if (status.is_disabled) {
                result = "rime-disable";
            } else if (status.is_ascii_mode) {
                result = "rime-latin";
            } else if (status.schema_id) {
                result = stringutils::concat("rime-im-", status.schema_id);
            } else {
                result = "rime-im";
            }
            engine_->api()->free_status(&status);
        }
        return result;
    }

private:
    RimeEngine *engine_;
};

RimeEngine::RimeEngine(Instance *instance)
    : instance_(instance), api_(rime_get_api()),
      factory_([this](InputContext &) { return new RimeState(this); }) {
    imAction_ = std::make_unique<IMAction>(this);
    instance_->userInterfaceManager().registerAction("rime-im",
                                                     imAction_.get());
    eventDispatcher_.attach(&instance_->eventLoop());
    deployAction_.setIcon("rime-deploy");
    deployAction_.setShortText(_("Deploy"));
    deployAction_.connect<SimpleAction::Activated>([this](InputContext *ic) {
        instance_->inputContextManager().foreach([this](InputContext *ic) {
            auto state = this->state(ic);
            state->release();
            return true;
        });
        api_->sync_user_data();
        api_->finalize();
        rimeStart(true);
        auto state = this->state(ic);
        if (ic->hasFocus()) {
            state->updateUI(ic, false);
        }
    });
    instance_->userInterfaceManager().registerAction("rime-deploy",
                                                     &deployAction_);

    syncAction_.setIcon(_("rime-sync"));
    syncAction_.setShortText(_("Synchronize"));

    syncAction_.connect<SimpleAction::Activated>([this](InputContext *ic) {
        api_->sync_user_data();
        auto state = this->state(ic);
        if (ic->hasFocus()) {
            state->updateUI(ic, false);
        }
    });
    instance_->userInterfaceManager().registerAction("rime-sync", &syncAction_);
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

    auto userDir = stringutils::joinPath(
        StandardPath::global().userDirectory(StandardPath::Type::PkgData),
        "rime");
    if (!fs::makePath(userDir)) {
        if (fs::isdir(userDir)) {
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
    if (firstRun_) {
        api_->setup(&fcitx_rime_traits);
        firstRun_ = false;
    }
    api_->initialize(&fcitx_rime_traits);
    api_->set_notification_handler(&rimeNotificationHandler, this);
    api_->start_maintenance(fullcheck);
}

void RimeEngine::reloadConfig() {
    factory_.unregister();
    if (api_) {
        try {
            api_->finalize();
        } catch (const std::exception &e) {
            RIME_ERROR() << e.what();
        }
    }
    rimeStart(false);
    instance_->inputContextManager().registerProperty("rimeState", &factory_);
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
    event.inputContext()->statusArea().clearGroup(StatusGroup::InputMethod);
    reset(entry, event);
}
void RimeEngine::keyEvent(const InputMethodEntry &entry, KeyEvent &event) {
    FCITX_UNUSED(entry);
    FCITX_LOG(Debug) << "Rime receive key: " << event.key() << " "
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
            auto notifications = that->notifications();
            if (!notifications) {
                return;
            }
            const char *message = nullptr;
            if (messageType == "deploy") {
                if (messageValue == "start") {
                    message = _("Rime is under maintenance. It may take a few "
                                "seconds. Please wait until it is finished...");
                } else if (messageValue == "success") {
                    message = _("Rime is ready.");
                } else if (messageValue == "failure") {
                    message = _("Rime has encountered an error. "
                                "See /tmp/rime.fcitx.ERROR for details.");
                }
            }

            if (message) {
                notifications->call<INotifications::showTip>(
                    "fcitx-rime-deploy", "fcitx", "rime-deploy", _("Rime"),
                    message, -1);
            }
        });
}

RimeState *RimeEngine::state(InputContext *ic) {
    return ic->propertyFor(&factory_);
}

std::string RimeEngine::subMode(const InputMethodEntry &, InputContext &ic) {
    auto rimeState = state(&ic);

    std::string result;
    RIME_STRUCT(RimeStatus, status);
    if (rimeState->getStatus(&status)) {
        if (status.schema_name) {
            result = status.schema_name;
        }
        api_->free_status(&status);
    }
    return result;
}
} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::RimeEngineFactory)
