/*
 * SPDX-FileCopyrightText: 2020~2020 xzhao9 <i@xuzhao.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "ConfigMain.h"
#include <QDialogButtonBox>
#include <QDir>
#include <QFlags>
#include <QFutureWatcher>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QTreeWidgetItem>
#include <QtGlobal>

namespace fcitx::rime {

ConfigMain::ConfigMain(QWidget *parent)
    : FcitxQtConfigUIWidget(parent),
      model_(std::make_unique<RimeConfigDataModel>()), inError_(false) {
    // Setup UI
    setupUi(this);

    // listViews for currentIM and availIM
    activeIMModel_ = std::make_unique<QStandardItemModel>(this);
    currentIMView->setModel(activeIMModel_.get());
    availIMModel_ = std::make_unique<QStandardItemModel>(this);
    availIMView->setModel(availIMModel_.get());

    // Shortcuts Tab
    connect(candidateWordNumber, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ConfigMain::stateChanged);
    connect(shiftLeftCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConfigMain::stateChanged);
    connect(shiftRightCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ConfigMain::stateChanged);
    auto keywgts = shortCutTab->findChildren<kcm::KeyListWidget *>();
    for (auto &keywgt : keywgts) {
        connect(keywgt, &kcm::KeyListWidget::keyChanged, this,
                &ConfigMain::keytoggleChanged);
    }

    // Schemas Tab
    connect(removeIMButton, &QPushButton::clicked, this, &ConfigMain::removeIM);
    connect(addIMButton, &QPushButton::clicked, this, &ConfigMain::addIM);
    connect(moveUpButton, &QPushButton::clicked, this, &ConfigMain::moveUpIM);
    connect(moveDownButton, &QPushButton::clicked, this,
            &ConfigMain::moveDownIM);
    connect(availIMView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &ConfigMain::availIMSelectionChanged);
    connect(currentIMView->selectionModel(),
            &QItemSelectionModel::currentChanged, this,
            &ConfigMain::activeIMSelectionChanged);

    if (!yamlToModel()) { // Load data from yaml
        disableUi(_("Failed to load Rime config or api. Please check your Rime "
                    "config or installation."));
        return;
    }

    modelToUi();
}

ConfigMain::~ConfigMain() {}

void ConfigMain::keytoggleChanged() { stateChanged(); }

// SLOTs
void ConfigMain::stateChanged() { emit changed(true); }

void ConfigMain::focusSelectedIM(const QString im_name) {
    // search enabled IM first
    int sz = activeIMModel_->rowCount();
    for (int i = 0; i < sz; i++) {
        QModelIndex ind = activeIMModel_->index(i, 0);
        const QString name =
            activeIMModel_->data(ind, Qt::DisplayRole).toString();
        if (name == im_name) {
            currentIMView->setCurrentIndex(ind);
            currentIMView->setFocus();
            return;
        }
    }
    // if not found, search avali IM list
    sz = availIMModel_->rowCount();
    for (int i = 0; i < sz; i++) {
        QModelIndex ind = availIMModel_->index(i, 0);
        const QString name =
            availIMModel_->data(ind, Qt::DisplayRole).toString();
        if (name == im_name) {
            availIMView->setCurrentIndex(ind);
            availIMView->setFocus();
            return;
        }
    }
}

void ConfigMain::addIM() {
    if (availIMView->currentIndex().isValid()) {
        const QString uniqueName =
            availIMView->currentIndex().data(Qt::DisplayRole).toString();
        int largest = 0;
        int find = -1;
        for (int i = 0; i < model_->schemas_.size(); i++) {
            if (model_->schemas_[i].name == uniqueName) {
                find = i;
            }
            if (model_->schemas_[i].index > largest) {
                largest = model_->schemas_[i].index;
            }
        }
        if (find != -1) {
            model_->schemas_[find].active = true;
            model_->schemas_[find].index = largest + 1;
        }

        model_->sortSchemas();
        updateIMList();
        focusSelectedIM(uniqueName);
        stateChanged();
    }
}

void ConfigMain::removeIM() {
    if (currentIMView->currentIndex().isValid()) {
        const QString uniqueName =
            currentIMView->currentIndex().data(Qt::DisplayRole).toString();
        for (int i = 0; i < model_->schemas_.size(); i++) {
            if (model_->schemas_[i].name == uniqueName) {
                model_->schemas_[i].active = false;
                model_->schemas_[i].index = 0;
            }
        }
        model_->sortSchemas();
        updateIMList();
        focusSelectedIM(uniqueName);
        stateChanged();
    }
}

void ConfigMain::moveUpIM() {
    if (currentIMView->currentIndex().isValid()) {
        const QString uniqueName =
            currentIMView->currentIndex().data(Qt::DisplayRole).toString();
        int curIndex = -1;
        for (int i = 0; i < model_->schemas_.size(); i++) {
            if (model_->schemas_[i].name == uniqueName) {
                curIndex = model_->schemas_[i].index;
                Q_ASSERT(curIndex == (i + 1)); // make sure the schema is sorted
            }
        }
        // can't move up the top schema because the button should be grey
        if (curIndex == -1 || curIndex == 0) {
            return;
        }

        int temp;
        temp = model_->schemas_[curIndex - 1].index;
        model_->schemas_[curIndex - 1].index =
            model_->schemas_[curIndex - 2].index;
        model_->schemas_[curIndex - 2].index = temp;
        model_->sortSchemas();
        updateIMList();
        focusSelectedIM(uniqueName);
        stateChanged();
    }
}

void ConfigMain::moveDownIM() {
    if (currentIMView->currentIndex().isValid()) {
        const QString uniqueName =
            currentIMView->currentIndex().data(Qt::DisplayRole).toString();
        int curIndex = -1;
        int temp;

        for (int i = 0; i < model_->schemas_.size(); i++) {
            if (model_->schemas_[i].name == uniqueName) {
                curIndex = model_->schemas_[i].index;
                Q_ASSERT(curIndex == (i + 1)); // make sure the schema is sorted
            }
        }
        // can't move down the bottom schema because the button should be grey
        if (curIndex == -1 || curIndex == 0) {
            return;
        }
        temp = model_->schemas_[curIndex - 1].index;
        model_->schemas_[curIndex - 1].index = model_->schemas_[curIndex].index;
        model_->schemas_[curIndex].index = temp;
        model_->sortSchemas();
        updateIMList();
        focusSelectedIM(uniqueName);
        stateChanged();
    }
}

void ConfigMain::availIMSelectionChanged() {
    if (!availIMView->currentIndex().isValid()) {
        addIMButton->setEnabled(false);
    } else {
        addIMButton->setEnabled(true);
    }
}

void ConfigMain::activeIMSelectionChanged() {
    if (!currentIMView->currentIndex().isValid()) {
        removeIMButton->setEnabled(false);
        moveUpButton->setEnabled(false);
        moveDownButton->setEnabled(false);
        // configureButton->setEnabled(false);
    } else {
        removeIMButton->setEnabled(true);
        // configureButton->setEnabled(true);
        if (currentIMView->currentIndex().row() == 0) {
            moveUpButton->setEnabled(false);
        } else {
            moveUpButton->setEnabled(true);
        }
        if (currentIMView->currentIndex().row() ==
            activeIMModel_->rowCount() - 1) {
            moveDownButton->setEnabled(false);
        } else {
            moveDownButton->setEnabled(true);
        }
    }
}
// end of SLOTs

QString ConfigMain::icon() { return "fcitx-rime"; }

QString ConfigMain::title() { return _("Fcitx5 Rime Config Tool"); }

void ConfigMain::load() {
    if (inError_) {
        return;
    }
    modelToUi();
}

void ConfigMain::disableUi(const QString &message) {
    setEnabled(false);
    QMetaObject::invokeMethod(
        this,
        [this, message]() { QMessageBox::critical(this, _("Error"), message); },
        Qt::QueuedConnection);
    inError_ = true;
}

void ConfigMain::uiToModel() {
    QModelIndex parent;
    int seqno = 1;

    model_->candidatePerWord_ = candidateWordNumber->value();

    model_->toggleKeys_ = toggle_shortcut->keys();
    model_->asciiKeys_ = hotkey_ascii->keys();
    model_->pgdownKeys_ = hotkey_pagedown->keys();
    model_->pgupKeys_ = hotkey_pageup->keys();
    model_->trasimKeys_ = hotkey_transim->keys();
    model_->halffullKeys_ = hotkey_hfshape->keys();

    if (model_->switchKeys_.size() >= 2) {
        model_->switchKeys_[0] =
            textToSwitchKey(shiftLeftCombo->currentIndex());
        model_->switchKeys_[1] =
            textToSwitchKey(shiftRightCombo->currentIndex());
    }

    // clear cuurent model and save from the ui
    for (int i = 0; i < model_->schemas_.size(); i++) {
        model_->schemas_[i].index = 0;
        model_->schemas_[i].active = false;
    }

    for (int r = 0; r < activeIMModel_->rowCount(parent); ++r) {
        QModelIndex index = activeIMModel_->index(r, 0, parent);
        QVariant name = activeIMModel_->data(index);
        for (int i = 0; i < model_->schemas_.size(); i++) {
            if (model_->schemas_[i].name == name) {
                model_->schemas_[i].index = seqno++;
                model_->schemas_[i].active = true;
            }
        }
    }
    model_->sortSchemas();
}

void ConfigMain::save() {
    if (inError_) {
        return;
    }
    uiToModel();

    modelToYaml();
    emit changed(false);
    emit saveFinished();
}

void ConfigMain::setSwitchKey(QComboBox *box, SwitchKeyFunction switchKey) {
    int index = -1;
    switch (switchKey) {
    case SwitchKeyFunction::Noop:
        index = 0;
        break;
    case SwitchKeyFunction::InlineASCII:
        index = 1;
        break;
    case SwitchKeyFunction::CommitText:
        index = 2;
        break;
    case SwitchKeyFunction::CommitCode:
        index = 3;
        break;
    case SwitchKeyFunction::Clear:
        index = 4;
        break;
    };
    box->setCurrentIndex(index);
}

SwitchKeyFunction ConfigMain::textToSwitchKey(int currentIndex) {
    switch (currentIndex) {
    case 0:
        return SwitchKeyFunction::Noop;
    case 1:
        return SwitchKeyFunction::InlineASCII;
    case 2:
        return SwitchKeyFunction::CommitText;
    case 3:
        return SwitchKeyFunction::CommitCode;
    case 4:
        return SwitchKeyFunction::Clear;
    default:
        return SwitchKeyFunction::Noop;
    }
}

void ConfigMain::modelToUi() {
    candidateWordNumber->setValue(model_->candidatePerWord_);

    // set shortcut keys
    toggle_shortcut->setKeys(model_->toggleKeys_);
    hotkey_pagedown->setKeys(model_->pgdownKeys_);
    hotkey_pageup->setKeys(model_->pgupKeys_);
    hotkey_ascii->setKeys(model_->asciiKeys_);
    hotkey_transim->setKeys(model_->trasimKeys_);
    hotkey_hfshape->setKeys(model_->halffullKeys_);

    // set switch keys
    if (model_->switchKeys_.size() >= 2) {
        setSwitchKey(shiftLeftCombo, model_->switchKeys_[0]);
        setSwitchKey(shiftRightCombo, model_->switchKeys_[1]);
    }

    // Clear both models
    activeIMModel_->clear();
    availIMModel_->clear();
    // Set available and enabled input methods
    for (int i = 0; i < model_->schemas_.size(); i++) {
        auto &schema = model_->schemas_[i];
        if (schema.active) {
            QStandardItem *activeSchema = new QStandardItem(schema.name);
            activeSchema->setEditable(false);
            activeIMModel_->appendRow(activeSchema);
        } else {
            QStandardItem *inactiveSchema = new QStandardItem(schema.name);
            inactiveSchema->setEditable(false);
            availIMModel_->appendRow(inactiveSchema);
        }
    }
}

void ConfigMain::updateIMList() {
    availIMModel_->removeRows(0, availIMModel_->rowCount());
    activeIMModel_->removeRows(0, activeIMModel_->rowCount());
    for (int i = 0; i < model_->schemas_.size(); i++) {
        auto &schema = model_->schemas_[i];
        if (schema.active) {
            QStandardItem *activeSchema = new QStandardItem(schema.name);
            activeSchema->setEditable(false);
            activeIMModel_->appendRow(activeSchema);
        } else {
            QStandardItem *inactiveSchema = new QStandardItem(schema.name);
            inactiveSchema->setEditable(false);
            availIMModel_->appendRow(inactiveSchema);
        }
    }
}

void ConfigMain::modelToYaml() {
    std::vector<std::string> toggleKeys;
    std::vector<std::string> schemaNames;

    config_.setPageSize(model_->candidatePerWord_);

    for (int i = 0; i < model_->toggleKeys_.size(); i++) {
        toggleKeys.push_back(model_->toggleKeys_[i].toString());
    }

    config_.setToggleKeys(toggleKeys);
    config_.setKeybindings(model_->getKeybindings());
    config_.setSwitchKeys(std::vector<SwitchKeyFunction>(
        model_->switchKeys_.begin(), model_->switchKeys_.end()));

    // set active schema list
    schemaNames.reserve(model_->schemas_.size());
    for (int i = 0; i < model_->schemas_.size(); i++) {
        if (model_->schemas_[i].index == 0) {
            break;
        } else {
            schemaNames.push_back(model_->schemas_[i].id.toStdString());
        }
    }
    config_.setSchemas(schemaNames);

    inError_ = !(config_.sync());
    return;
}

bool ConfigMain::yamlToModel() {
    if (config_.isError()) {
        return false;
    }

    // load page size
    int pageSize = 0;
    if (config_.getPageSize(&pageSize)) {
        model_->candidatePerWord_ = pageSize;
    } else {
        model_->candidatePerWord_ = defaultPageSize;
    }

    // load toggle keys
    auto toggleKeys = config_.getToggleKeys();
    for (const auto &toggleKey : toggleKeys) {
        if (!toggleKey.empty()) { // skip the empty keys
            Key data = Key(toggleKey.data());
            model_->toggleKeys_.push_back(std::move(data));
        }
    }

    // load keybindings
    auto bindings = config_.getKeybindings();
    model_->setKeybindings(std::move(bindings));

    // load switchkeys
    auto switchKeys = config_.getSwitchKeys();
    model_->switchKeys_ =
        QList<SwitchKeyFunction>(switchKeys.begin(), switchKeys.end());

    // load schemas
    getAvailableSchemas();
    return true;
}

void ConfigMain::getAvailableSchemas() {
    const char *userPath = RimeGetUserDataDir();
    const char *sysPath = RimeGetSharedDataDir();

    QSet<QString> files;
    for (auto path : {sysPath, userPath}) {
        if (!path) {
            continue;
        }
        QDir dir(path);
        QList<QString> entryList = dir.entryList(QStringList("*.schema.yaml"),
                                                 QDir::Files | QDir::Readable);
        files.unite(QSet<QString>(entryList.begin(), entryList.end()));
    }

    auto filesList = files.values();
    filesList.sort();

    for (const auto &file : filesList) {
        auto schema = FcitxRimeSchema();
        QString fullPath;
        for (auto path : {userPath, sysPath}) {
            QDir dir(path);
            if (dir.exists(file)) {
                fullPath = dir.filePath(file);
                break;
            }
        }
        schema.path = fullPath;
        QFile fd(fullPath);
        if (!fd.open(QIODevice::ReadOnly)) {
            continue;
        }
        auto yamlData = fd.readAll();
        auto name = config_.stringFromYAML(yamlData.constData(), "schema/name");
        auto id =
            config_.stringFromYAML(yamlData.constData(), "schema/schema_id");
        schema.name = QString::fromStdString(name);
        schema.id = QString::fromStdString(id);
        schema.index = config_.schemaIndex(id.data());
        schema.active = static_cast<bool>(schema.index);
        model_->schemas_.push_back(schema);
    }
    model_->sortSchemas();
}

} // namespace fcitx::rime
