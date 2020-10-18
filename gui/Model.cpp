/*
 * SPDX-FileCopyrightText: 2020~2020 xzhao9 <i@xuzhao.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "Model.h"
#include <algorithm>
#include <fcitxqtkeysequencewidget.h>
#include <iostream>

namespace fcitx::rime {

void RimeConfigDataModel::sortSchemas() {
    std::sort(schemas_.begin(), schemas_.end(),
              [](const FcitxRimeSchema &a, const FcitxRimeSchema &b) -> bool {
                  // if both inactive, sort by id
                  if (a.index == 0 && b.index == 0) {
                      return a.id < b.id;
                  }
                  if (a.index == 0) {
                      return false;
                  }
                  if (b.index == 0) {
                      return true;
                  }
                  return (a.index < b.index);
              });
}

void RimeConfigDataModel::sortKeys() {
    sortSingleKeySet(toggleKeys_);
    sortSingleKeySet(asciiKeys_);
    sortSingleKeySet(trasimKeys_);
    sortSingleKeySet(halffullKeys_);
    sortSingleKeySet(pgupKeys_);
    sortSingleKeySet(pgdownKeys_);
}

void RimeConfigDataModel::sortSingleKeySet(QList<Key> &keys) {
    std::sort(keys.begin(), keys.end(), [](const Key &a, const Key &b) -> bool {
        return a.toString() < b.toString();
    });
    std::unique(keys.begin(), keys.end());
}

void RimeConfigDataModel::setKeybindings(std::vector<KeyBinding> bindings) {
    for (const auto &binding : bindings) {
        if (binding.accept.empty()) {
            continue;
        }
        if (binding.action == "ascii_mode") {
            Key seq(binding.accept);
            asciiKeys_.push_back(seq);
        } else if (binding.action == "full_shape") {
            Key seq(binding.accept);
            halffullKeys_.push_back(seq);
        } else if (binding.action == "simplification") {
            Key seq(binding.accept);
            trasimKeys_.push_back(seq);
        } else if (binding.action == "Page_Up") {
            Key seq(binding.accept);
            pgupKeys_.push_back(seq);
        } else if (binding.action == "Page_Down") {
            Key seq(binding.accept);
            pgdownKeys_.push_back(seq);
        }
    }
    sortKeys();
}

std::vector<KeyBinding> RimeConfigDataModel::getKeybindings() {
    std::vector<KeyBinding> out;
    // Fill ascii_key
    for (auto &ascii : asciiKeys_) {
        KeyBinding binding;
        binding.action = "ascii_mode";
        binding.when = KeyBindingCondition::Always;
        binding.type = KeyBindingType::Toggle;
        binding.accept = ascii.toString();
        out.push_back(binding);
    }
    // Fill trasim_key
    for (auto &trasim : trasimKeys_) {
        KeyBinding binding;
        binding.action = "simplification";
        binding.when = KeyBindingCondition::Always;
        binding.type = KeyBindingType::Toggle;
        binding.accept = trasim.toString();
        out.push_back(binding);
    }
    // Fill halffull_key
    for (auto &halffull : halffullKeys_) {
        KeyBinding binding;
        binding.action = "full_shape";
        binding.when = KeyBindingCondition::Always;
        binding.type = KeyBindingType::Toggle;
        binding.accept = halffull.toString();
        out.push_back(binding);
    }
    // Fill pgup_key
    for (auto &pgup : pgupKeys_) {
        KeyBinding binding;
        binding.action = "Page_Up";
        binding.when = KeyBindingCondition::HasMenu;
        binding.type = KeyBindingType::Send;
        binding.accept = pgup.toString();
        out.push_back(binding);
    }
    // Fill pgdown_key
    for (auto &pgup : pgdownKeys_) {
        KeyBinding binding;
        binding.action = "Page_Down";
        binding.when = KeyBindingCondition::HasMenu;
        binding.type = KeyBindingType::Send;
        binding.accept = pgup.toString();
        out.push_back(binding);
    }
    return out;
}

} // namespace fcitx::rime
