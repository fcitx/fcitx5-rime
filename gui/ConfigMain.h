/*
 * SPDX-FileCopyrightText: 2020~2020 xzhao9 <i@xuzhao.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _RIME_GUI_CONFIGMAIN_H
#define _RIME_GUI_CONFIGMAIN_H

#include <QStandardItemModel>
#include <fcitx-utils/key.h>
#include <fcitxqtconfiguiwidget.h>
#include <fcitxqtkeysequencewidget.h>

#include "Model.h"
#include "RimeConfigParser.h"
#include "ui_ConfigMain.h"

namespace fcitx::rime {

class ConfigMain : public FcitxQtConfigUIWidget, private Ui::MainUI {
    Q_OBJECT
public:
    explicit ConfigMain(QWidget *parent = 0);
    QString title() override;
    ~ConfigMain();
    void load() override;
    void save() override;
    bool asyncSave() override { return true; }

    QString icon() override;
public slots:
    void keytoggleChanged();
    void stateChanged();
    void addIM();
    void removeIM();
    void moveUpIM();
    void moveDownIM();
    void availIMSelectionChanged();
    void activeIMSelectionChanged();

private:
    void disableUi(const QString &message);
    bool yamlToModel();
    void uiToModel();
    void modelToUi();
    void modelToYaml();
    void getAvailableSchemas();
    void updateIMList();
    void focusSelectedIM(const QString im_name);
    void setSwitchKey(QComboBox *box, SwitchKeyFunction switchKey);
    SwitchKeyFunction textToSwitchKey(int currentIndex);

    RimeConfigParser config_;
    std::unique_ptr<RimeConfigDataModel> model_;
    std::unique_ptr<QStandardItemModel> activeIMModel_;
    std::unique_ptr<QStandardItemModel> availIMModel_;

    bool inError_;
};

} // namespace fcitx::rime

#endif // _RIME_GUI_CONFIGMAIN_H
