/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */
#include "rimesession.h"
#include "rimeengine.h"
#include <cassert>
#include <fcitx-utils/charutils.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/macros.h>
#include <fcitx-utils/stringutils.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <memory>
#include <rime_api.h>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace fcitx {

RimeSessionHolder::RimeSessionHolder(RimeSessionPool *pool,
                                     const std::string &program)
    : pool_(pool) {
    auto *api = pool_->engine()->api();
    id_ = api->create_session();

    if (!id_) {
        throw std::runtime_error("Failed to create session.");
    }

    setProgramName(program);

    if (program.empty()) {
        return;
    }

    const auto &appOptions = pool_->engine()->appOptions();
    if (auto iter = appOptions.find(program); iter != appOptions.end()) {
        RIME_DEBUG() << "Apply app options to " << program << ": "
                     << iter->second;
        for (const auto &[key, value] : iter->second) {
            api->set_option(id_, key.data(), value);
        }
    }
}

RimeSessionHolder::~RimeSessionHolder() {
    if (id_) {
        pool_->engine()->api()->destroy_session(id_);
    }
    if (!key_.empty()) {
        pool_->unregisterSession(key_);
    }
}

void RimeSessionHolder::setProgramName(const std::string &program) {
    // set_property will trigger property change notification, which is a little
    // bit annoying, so don't set it, if the value is not changed.
    if (program == currentProgram_) {
        return;
    }

    currentProgram_ = program;
    pool_->engine()->api()->set_property(id_, "client_app", program.data());
}

#if 0
LogMessageBuilder &operator<<(LogMessageBuilder &log, const std::weak_ptr<RimeSessionHolder> &session) {
    auto sessionPtr = session.lock();
    log << "RimeSession("<< (sessionPtr ? std::to_string(sessionPtr->id()) : "null")  << ")";
    return log;
}
#endif

RimeSessionPool::RimeSessionPool(RimeEngine *engine,
                                 PropertyPropagatePolicy initialPolicy)
    : engine_(engine), policy_(initialPolicy) {}

void RimeSessionPool::setPropertyPropagatePolicy(
    PropertyPropagatePolicy policy) {
    if (policy_ == policy) {
        return;
    }

    assert(sessions_.empty());
    policy_ = policy;
}

std::string uuidKey(InputContext *ic) {
    std::string key = "u:";
    for (auto v : ic->uuid()) {
        auto lower = v % 16;
        auto upper = v / 16;
        key.push_back(charutils::toHex(upper));
        key.push_back(charutils::toHex(lower));
    }
    return key;
}

std::tuple<std::shared_ptr<RimeSessionHolder>, bool>
RimeSessionPool::requestSession(InputContext *ic) {
    std::string key;
    switch (policy_) {
    case PropertyPropagatePolicy::No:
        key = uuidKey(ic);
        break;
    case PropertyPropagatePolicy::Program:
        if (!ic->program().empty()) {
            key = stringutils::concat("p:", ic->program());
        } else {
            key = uuidKey(ic);
        }
        break;
    case PropertyPropagatePolicy::All:
        key = "g:";
        break;
    }
    auto iter = sessions_.find(key);
    if (iter != sessions_.end()) {
        return {iter->second.lock(), false};
    }
    try {
        auto newSession =
            std::make_shared<RimeSessionHolder>(this, ic->program());
        registerSession(key, newSession);
        return {newSession, true};
    } catch (...) {
    }
    return {nullptr, false};
}

void RimeSessionPool::registerSession(
    const std::string &key, std::shared_ptr<RimeSessionHolder> session) {
    assert(!key.empty());
    session->key_ = key;
    auto [_, success] = sessions_.emplace(key, session);
    FCITX_UNUSED(success);
    assert(success);
}

void RimeSessionPool::unregisterSession(const std::string &key) {
    auto count = sessions_.erase(key);
    FCITX_UNUSED(count);
    assert(count > 0);
}

} // namespace fcitx
