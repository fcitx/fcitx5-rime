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
#ifndef _FCITX_RIMEENGINE_H_
#define _FCITX_RIMEENGINE_H_

#include <fcitx-config/configuration.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/log.h>
#include <fcitx/action.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx/menu.h>
#include <memory>
#include <rime_api.h>

namespace fcitx {

class RimeState;

FCITX_CONFIGURATION(RimeEngineConfig,
                    Option<bool> showPreeditInApplication{
                        this, "PreeditInApplication",
                        "Use preedit in application when possible", false};);

class RimeEngine final : public InputMethodEngine {
public:
    RimeEngine(Instance *instance);
    ~RimeEngine();
    Instance *instance() { return instance_; }
    void activate(const InputMethodEntry &entry,
                  InputContextEvent &event) override;
    void deactivate(const InputMethodEntry &entry,
                    InputContextEvent &event) override;
    void keyEvent(const InputMethodEntry &entry, KeyEvent &keyEvent) override;
    void reloadConfig() override;
    void reset(const InputMethodEntry &entry,
               InputContextEvent &event) override;
    void save() override;
    auto &factory() { return factory_; }

    void updateAction(InputContext *inputContext) {
        imAction_->update(inputContext);
    }

    const Configuration *getConfig() const override { return &config_; }
    void setConfig(const RawConfig &config) override {
        config_.load(config, true);
        safeSaveAsIni(config_, "conf/rime.conf");
        reloadConfig();
    }

    std::string subMode(const InputMethodEntry &, InputContext &) override;
    const RimeEngineConfig &config() const { return config_; }

    rime_api_t *api() { return api_; }

    void rimeStart(bool fullcheck);

    RimeState *state(InputContext *ic);

private:
    static void rimeNotificationHandler(void *context_object,
                                        RimeSessionId session_id,
                                        const char *message_type,
                                        const char *message_value);

    Instance *instance_;
    EventDispatcher eventDispatcher_;
    rime_api_t *api_;
    bool firstRun_ = true;
    FactoryFor<RimeState> factory_;

    std::unique_ptr<Action> imAction_;
    SimpleAction deployAction_;
    SimpleAction syncAction_;

    Menu imMenu_;
    RimeEngineConfig config_;

    FCITX_ADDON_DEPENDENCY_LOADER(notifications, instance_->addonManager());
};

class RimeEngineFactory : public AddonFactory {
public:
    AddonInstance *create(AddonManager *manager) override {
        return new RimeEngine(manager->instance());
    }
};
} // namespace fcitx

FCITX_DECLARE_LOG_CATEGORY(rime);

#define RIME_DEBUG() FCITX_LOGC(rime, Debug)
#define RIME_ERROR() FCITX_LOGC(rime, Error)

#endif // _FCITX_RIMEENGINE_H_
