/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _FCITX_RIMEENGINE_H_
#define _FCITX_RIMEENGINE_H_

#ifndef FCITX_RIME_NO_DBUS
#include "rimeservice.h"
#endif
#include "rimesession.h"
#include "rimestate.h"
#include <fcitx-config/configuration.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/handlertable_details.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/library.h>
#include <fcitx-utils/log.h>
#include <fcitx/action.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/icontheme.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx/menu.h>
#include <memory>
#include <rime_api.h>

namespace fcitx {

class RimeState;

FCITX_CONFIGURATION(
    RimeEngineConfig,
    Option<bool> showPreeditInApplication{this, "PreeditInApplication",
                                          _("Show preedit within application"),
                                          true};
    Option<bool> preeditCursorPositionAtBeginning{
        this, "PreeditCursorPositionAtBeginning",
        _("Fix embedded preedit cursor at the beginning of the preedit"), true};
    Option<bool> commitWhenDeactivate{
        this, "Commit when deactivate",
        _("Commit current text when deactivating"), true};
    ExternalOption userDataDir{
        this, "UserDataDir", _("User data dir"),
        stringutils::concat(
            "xdg-open \"",
            stringutils::replaceAll(
                stringutils::joinPath(StandardPath::global().userDirectory(
                                          StandardPath::Type::PkgData),
                                      "rime"),
                "\"", "\"\"\""),
            "\"")};
#ifdef FCITX_RIME_LOAD_PLUGIN
    Option<bool> autoloadPlugins{this, "AutoloadPlugins",
                                 _("Load available plugins automatically"),
                                 false};
    Option<std::vector<std::string>> plugins{this, "Plugins", _("Plugins"),
                                             std::vector<std::string>()};
    Option<std::vector<std::string>> modules{this, "Modules", _("Modules"),
                                             std::vector<std::string>()};
#endif
);

class RimeEngine final : public InputMethodEngineV2 {
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
        updateConfig();
    }
    void setSubConfig(const std::string &path, const RawConfig &) override;
    void updateConfig();

    std::string subMode(const InputMethodEntry &, InputContext &) override;
    std::string subModeIconImpl(const InputMethodEntry &,
                                InputContext &) override;
    std::string subModeLabelImpl(const InputMethodEntry &,
                                 InputContext &) override;
    const RimeEngineConfig &config() const { return config_; }

    rime_api_t *api() { return api_; }
    const auto &appOptions() const { return appOptions_; }

    void rimeStart(bool fullcheck);

    RimeState *state(InputContext *ic);
    RimeSessionPool &sessionPool() { return sessionPool_; }

#ifndef FCITX_RIME_NO_DBUS
    FCITX_ADDON_DEPENDENCY_LOADER(dbus, instance_->addonManager());
#endif

private:
    static void rimeNotificationHandler(void *context_object,
                                        RimeSessionId session_id,
                                        const char *message_type,
                                        const char *message_value);

    void deploy();
    void sync();
    void updateSchemaMenu();
    void notify(RimeSessionId session, const std::string &type,
                const std::string &value);
    void releaseAllSession();
    void updateAppOptions();
    void refreshStatusArea(InputContext &ic);
    void refreshStatusArea(RimeSessionId session);

    IconTheme theme_;
    Instance *instance_;
    EventDispatcher eventDispatcher_;
    rime_api_t *api_;
    bool firstRun_ = true;
    uint64_t blockNotificationBefore_ = 0;
    FactoryFor<RimeState> factory_;

    std::unique_ptr<Action> imAction_;
    SimpleAction separatorAction_;
    SimpleAction deployAction_;
    SimpleAction syncAction_;

    RimeEngineConfig config_;
    std::unordered_map<std::string, std::unordered_map<std::string, bool>>
        appOptions_;

    FCITX_ADDON_DEPENDENCY_LOADER(notifications, instance_->addonManager());

    std::list<SimpleAction> schemActions_;
    std::list<std::unique_ptr<Action>> optionActions_;
    Menu schemaMenu_;
#ifdef FCITX_RIME_LOAD_PLUGIN
    std::unordered_map<std::string, Library> pluginPool_;
#endif
    std::unique_ptr<EventSourceTime> timeEvent_;
    std::unique_ptr<HandlerTableEntry<EventHandler>> globalConfigReloadHandle_;

#ifndef FCITX_RIME_NO_DBUS
    RimeService service_{this};
#endif
    RimeSessionPool sessionPool_;
};

class RimeEngineFactory : public AddonFactory {
public:
    AddonInstance *create(AddonManager *manager) override {
        registerDomain("fcitx5-rime", FCITX_INSTALL_LOCALEDIR);
        return new RimeEngine(manager->instance());
    }
};
} // namespace fcitx

FCITX_DECLARE_LOG_CATEGORY(rime);

#define RIME_DEBUG() FCITX_LOGC(rime, Debug)
#define RIME_ERROR() FCITX_LOGC(rime, Error)

#endif // _FCITX_RIMEENGINE_H_
