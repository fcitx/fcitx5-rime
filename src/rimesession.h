/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */
#ifndef _FCITX5_RIME_RIMESESSION_H_
#define _FCITX5_RIME_RIMESESSION_H_

#include <fcitx-utils/log.h>
#include <fcitx-utils/macros.h>
#include <fcitx/inputcontextmanager.h>
#include <memory>
#include <rime_api.h>
#include <string>

namespace fcitx {

class RimeEngine;
class RimeSessionPool;

class RimeSessionHolder {
    friend class RimeSessionPool;

public:
    RimeSessionHolder(RimeSessionPool *pool, const std::string &program);

    RimeSessionHolder(RimeSessionHolder &&) = delete;

    ~RimeSessionHolder();

    RimeSessionId id() const { return id_; }

private:
    RimeSessionPool *pool_;
    RimeSessionId id_ = 0;
    std::string key_;
};

class RimeSessionPool {
    friend class RimeSessionHolder;

public:
    RimeSessionPool(RimeEngine *engine, PropertyPropagatePolicy initialPolicy);

    void setPropertyPropagatePolicy(PropertyPropagatePolicy policy);

    std::shared_ptr<RimeSessionHolder> requestSession(InputContext *ic);

    RimeEngine *engine() const { return engine_; }

private:
    void registerSession(const std::string &key,
                         std::shared_ptr<RimeSessionHolder> session);
    void unregisterSession(const std::string &key);
    RimeEngine *engine_;
    PropertyPropagatePolicy policy_;
    std::unordered_map<std::string, std::weak_ptr<RimeSessionHolder>> sessions_;
};

} // namespace fcitx

#endif