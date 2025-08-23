#include "CheatEngine.h"
#include "CoreController.h"
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRandomGenerator>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <qstatusbar.h>

extern "C" {
#include <mgba/core/cheats.h>
#include <mgba/core/core.h>
}

using QGBA::CoreController;

CheatEngine::CheatEngine(std::shared_ptr<CoreController> controller, QWidget* parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_controller(std::move(controller))
    , m_cheatDevice(nullptr) {
	setupUI();
	initCheatDevice();
	setupHotkeys();
	setupFrameCallback();
	updateCheatTable();
}
CheatEngine::~CheatEngine() {
	removeFrameCallback();
	for (auto& cheat : m_cheats) {
		if (cheat.cheatSet) {
			removeCheat(cheat);
		}
	}

	autoExportCheats();
}

void CheatEngine::setupUI() {
	setWindowTitle("GBA Cheat Engine");
	setMinimumSize(600, 500);

	m_centralWidget = new QWidget;
	setCentralWidget(m_centralWidget);

	m_mainSplitter = new QSplitter(Qt::Vertical);
	setupCheatManager();

	m_mainSplitter->addWidget(m_cheatGroup);
	m_mainSplitter->addWidget(m_editorGroup);

	m_logOutput = new QTextEdit;
	m_logOutput->setMaximumHeight(100);
	m_logOutput->setReadOnly(true);
	m_logOutput->append("Cheat Engine started...");

	m_statusLabel = new QLabel("Ready");
	statusBar()->addWidget(m_statusLabel);

	QVBoxLayout* mainLayout = new QVBoxLayout;
	mainLayout->addWidget(m_mainSplitter);
	mainLayout->addWidget(m_logOutput);

	m_centralWidget->setLayout(mainLayout);

	m_mainSplitter->setSizes({ 400, 150 });
}

void CheatEngine::setupCheatManager() {
	m_cheatGroup = new QGroupBox("Cheat Manager");

	m_cheatTable = new QTableWidget(0, 5);
	QStringList headers = { "Active", "Address", "Value", "Description", "Frozen" };
	m_cheatTable->setHorizontalHeaderLabels(headers);
	m_cheatTable->horizontalHeader()->setStretchLastSection(true);
	m_cheatTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_cheatTable->setAlternatingRowColors(true);

	QHBoxLayout* buttonLayout = new QHBoxLayout;
	m_addCheatButton = new QPushButton("Add");
	m_removeCheatButton = new QPushButton("Delete");
	m_importMapButton = new QPushButton("Import Map");
	m_exportMapButton = new QPushButton("Export Map");

	buttonLayout->addWidget(m_addCheatButton);
	buttonLayout->addWidget(m_removeCheatButton);
	buttonLayout->addWidget(m_importMapButton);
	buttonLayout->addWidget(m_exportMapButton);
	buttonLayout->addStretch();

	m_editorGroup = new QGroupBox("Cheat Editor");
	QGridLayout* editorLayout = new QGridLayout;

	editorLayout->addWidget(new QLabel("Adress:"), 0, 0);
	m_cheatAddress = new QLineEdit;
	m_cheatAddress->setPlaceholderText("0x8000");
	editorLayout->addWidget(m_cheatAddress, 0, 1);

	editorLayout->addWidget(new QLabel("Value:"), 1, 0);
	m_cheatValue = new QLineEdit;
	m_cheatValue->setPlaceholderText("255");
	editorLayout->addWidget(m_cheatValue, 1, 1);

	editorLayout->addWidget(new QLabel("Description:"), 2, 0);
	m_cheatDescription = new QLineEdit;
	m_cheatDescription->setPlaceholderText("Infinite Lives");
	editorLayout->addWidget(m_cheatDescription, 2, 1);

	editorLayout->addWidget(new QLabel("Type:"), 3, 0);
	m_cheatType = new QComboBox;
	m_cheatType->addItems({ "Byte", "Word" });
	editorLayout->addWidget(m_cheatType, 3, 1);

	m_freezeCheckbox = new QCheckBox("Freeze value");
	editorLayout->addWidget(m_freezeCheckbox, 4, 0, 1, 2);

	m_editorGroup->setLayout(editorLayout);

	QVBoxLayout* layout = new QVBoxLayout;
	layout->addWidget(m_cheatTable);
	layout->addLayout(buttonLayout);

	m_cheatGroup->setLayout(layout);

	connect(m_addCheatButton, &QPushButton::clicked, this, &CheatEngine::onAddCheat);
	connect(m_removeCheatButton, &QPushButton::clicked, this, &CheatEngine::onRemoveCheat);
	connect(m_importMapButton, &QPushButton::clicked, this, &CheatEngine::onImportMap);
	connect(m_exportMapButton, &QPushButton::clicked, this, &CheatEngine::onExportMap);
	connect(m_cheatTable, &QTableWidget::itemSelectionChanged, this, &CheatEngine::onCheatTableSelectionChanged);
}

void CheatEngine::onAddCheat() {
	QString address = m_cheatAddress->text();
	QString value = m_cheatValue->text();
	QString description = m_cheatDescription->text();
	QString type = m_cheatType->currentText();
	bool frozen = m_freezeCheckbox->isChecked();

	if (address.isEmpty() || value.isEmpty()) {
		m_logOutput->append("Error: Address and value must be filled in");
		return;
	}

	CheatEntry cheat;
	cheat.address = address;
	cheat.value = value;
	cheat.description = description.isEmpty() ? "Unknown" : description;
	cheat.type = type;
	cheat.frozen = frozen;
	cheat.enabled = true;
	cheat.cheatSet = nullptr;

	if (cheat.enabled) {
		applyCheat(cheat);
	}

	m_cheats.append(cheat);
	updateCheatTable();

	m_logOutput->append(QString("Cheat added: %1 = %2").arg(address, value));

	m_cheatAddress->clear();
	m_cheatValue->clear();
	m_cheatDescription->clear();
	m_freezeCheckbox->setChecked(false);
}

void CheatEngine::onRemoveCheat() {
	int row = m_cheatTable->currentRow();
	if (row >= 0 && row < m_cheats.count()) {
		CheatEntry& cheat = m_cheats[row];

		removeCheat(cheat);

		m_logOutput->append(QString("Cheat deleted: %1").arg(cheat.description));
		m_cheats.removeAt(row);
		updateCheatTable();
	}
}

void CheatEngine::onImportMap() {
	QString fileName =
	    QFileDialog::getOpenFileName(this, "Import Cheat Map", "", "mGBA Table Files (*.mgbatable);;All Files (*)");

	if (fileName.isEmpty()) {
		return;
	}

	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		QMessageBox::warning(this, "Error", "Could not open file: " + fileName);
		return;
	}

	QByteArray data = file.readAll();
	file.close();

	QJsonParseError error;
	QJsonDocument doc = QJsonDocument::fromJson(data, &error);

	if (error.error != QJsonParseError::NoError) {
		QMessageBox::warning(this, "Parse Error", QString("JSON parse error: %1").arg(error.errorString()));
		return;
	}

	if (!doc.isObject()) {
		QMessageBox::warning(this, "Format Error", "File must contain a JSON object");
		return;
	}

	QJsonObject root = doc.object();

	if (!root.contains("game") || !root.contains("cheats") || !root["cheats"].isArray()) {
		QMessageBox::warning(this, "Format Error", "Invalid .mgbatable file: Missing required fields");
		return;
	}

	QString gameName = root.value("game").toString("Unknown Game");
	QString version = root.value("version").toString("1.0");
	QString author = root.value("author").toString("Unknown");

	QString currentGame = getCurrentGameName();

	if (!currentGame.isEmpty() && !gameName.isEmpty() && currentGame != gameName) {
		QMessageBox msgBox(this);
		msgBox.setWindowTitle("Game mismatch");
		msgBox.setIcon(QMessageBox::Warning);
		msgBox.setText(QString("This table is for %1, but you are running %2").arg(gameName, currentGame));

		QPushButton* importAnywayBtn = msgBox.addButton("Import Anyway", QMessageBox::AcceptRole);
		QPushButton* cancelBtn = msgBox.addButton(QMessageBox::Cancel);

		msgBox.exec();

		if (msgBox.clickedButton() == cancelBtn) {
			return;
		} else if (msgBox.clickedButton() == importAnywayBtn) {
			m_logOutput->append("⚠ Importing anyway despite game mismatch.");
		}
	}

	m_logOutput->append(QString("Importing cheats for: %1 (v%2) by %3").arg(gameName, version, author));

	QJsonArray cheatsArray = root.value("cheats").toArray();
	int importedCount = 0;

	for (const QJsonValue& cheatValue : cheatsArray) {
		if (!cheatValue.isObject()) {
			continue;
		}

		QJsonObject cheatObj = cheatValue.toObject();

		if (!cheatObj.contains("address") || !cheatObj.contains("value")) {
			m_logOutput->append("Skipping invalid cheat entry (missing address or value)");
			continue;
		}

		QString address = cheatObj.value("address").toString();
		QString value = cheatObj.value("value").toString();
		QString description = cheatObj.value("description").toString();
		QString type = cheatObj.value("type").toString("Byte");
		bool frozen = cheatObj.value("frozen").toBool(false);
		bool enabled = cheatObj.value("enabled").toBool(true);

		if (address.isEmpty() || value.isEmpty()) {
			m_logOutput->append("Skipping invalid cheat entry (missing address or value)");
			continue;
		}

		CheatEntry cheat;
		cheat.address = address;
		cheat.value = value;
		cheat.description = description.isEmpty() ? "Imported Cheat" : description;
		cheat.type = type;
		cheat.frozen = frozen;
		cheat.enabled = enabled;
		cheat.cheatSet = nullptr;

		if (cheat.enabled) {
			applyCheat(cheat);
		}

		m_cheats.append(cheat);
		importedCount++;
	}

	updateCheatTable();

	m_logOutput->append(
	    QString("Successfully imported %1 cheats from %2").arg(importedCount).arg(QFileInfo(fileName).fileName()));

	if (importedCount > 0) {
		QMessageBox::information(this, "Import Complete",
		                         QString("Successfully imported %1 cheats!").arg(importedCount));
	} else {
		QMessageBox::warning(this, "Import Warning", "No valid cheats found in file");
	}
}

QString CheatEngine::getCurrentGameName() {
	mCore* core = m_controller->thread()->core;
	if (!core)
		return "Unknown Game";

	mGameInfo info {};
	if (core->getGameInfo) {
		core->getGameInfo(core, &info);
		if (info.title[0]) {
			return QString::fromUtf8(info.title);
		}
	}

	return "Unknown Game";
}

void CheatEngine::onExportMap() {
	QString fileName =
	    QFileDialog::getSaveFileName(this, "Export Cheat Map", "", "mGBA Table Files (*.mgbatable);;All Files (*)");

	if (fileName.isEmpty())
		return;

	QJsonObject root;

	root["game"] = getCurrentGameName();
	root["version"] = "1.0";
	root["author"] = "User";

	QJsonArray cheatsArray;
	for (const auto& cheat : m_cheats) {
		QJsonObject cheatObj;
		cheatObj["address"] = cheat.address;
		cheatObj["value"] = cheat.value;
		cheatObj["description"] = cheat.description;
		cheatObj["type"] = cheat.type;
		cheatObj["frozen"] = cheat.frozen;
		cheatObj["enabled"] = cheat.enabled;
		cheatsArray.append(cheatObj);
	}
	root["cheats"] = cheatsArray;

	QJsonDocument doc(root);
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly)) {
		QMessageBox::warning(this, "Error", "Could not write file: " + fileName);
		return;
	}

	file.write(doc.toJson());
	file.close();

	m_logOutput->append(QString("Cheat map exported: %1").arg(fileName));
	QMessageBox::information(this, "Export Complete",
	                         QString("Successfully exported %1 cheats!").arg(cheatsArray.count()));
}

void CheatEngine::autoExportCheats() {
	QString fileName = QDir::homePath() + "/.mgba/autosave.mgbatable";

	QDir dir(QDir::homePath() + "/.mgba");
	if (!dir.exists()) {
		dir.mkpath(".");
	}

	QJsonObject root;

	root["game"] = getCurrentGameName();
	root["version"] = "1.0";
	root["author"] = "AutoSave";

	QJsonArray cheatsArray;
	for (const auto& cheat : m_cheats) {
		QJsonObject cheatObj;
		cheatObj["address"] = cheat.address;
		cheatObj["value"] = cheat.value;
		cheatObj["description"] = cheat.description;
		cheatObj["type"] = cheat.type;
		cheatObj["frozen"] = cheat.frozen;
		cheatObj["enabled"] = cheat.enabled;
		cheatsArray.append(cheatObj);
	}
	root["cheats"] = cheatsArray;

	QJsonDocument doc(root);
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly)) {
		qWarning() << "Could not auto-export cheats to " << fileName;
		return;
	}

	file.write(doc.toJson());
	file.close();

	qDebug() << "Auto-exported cheats to " << fileName;
}

void CheatEngine::onCheatTableSelectionChanged() {
	int row = m_cheatTable->currentRow();
	if (row >= 0 && row < m_cheats.count()) {
		CheatEntry cheat = m_cheats.at(row);
		m_cheatAddress->setText(cheat.address);
		m_cheatValue->setText(cheat.value);
		m_cheatDescription->setText(cheat.description);
		m_cheatType->setCurrentText(cheat.type);
		m_freezeCheckbox->setChecked(cheat.frozen);
	}
}

void CheatEngine::updateCheatTable()
{
    static bool connected = false;
    if (!connected) {
        connected = true;
        connect(m_cheatTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
            int row = item->row();
            if (row < 0 || row >= m_cheats.count()) return;

            CheatEntry& cheat = m_cheats[row];

            if (item->column() == 2) { 
                QString newValue = item->text();
                cheat.value = newValue;

                if (cheat.enabled && cheat.cheatSet) {
                    removeCheat(cheat);
                    applyCheat(cheat);
                }
                m_logOutput->append(QString("Updated cheat value: %1 = %2")
                                    .arg(cheat.address, newValue));
            }
            else if (item->column() == 3) {
                cheat.description = item->text();
                m_logOutput->append(QString("Updated cheat description: %1").arg(cheat.description));
            }
        });
    }


    m_cheatTable->blockSignals(true);
    m_cheatTable->setRowCount(m_cheats.count());

    for (int i = 0; i < m_cheats.count(); i++) {
        CheatEntry& cheat = m_cheats[i];

        QCheckBox *enabledBox = new QCheckBox;
        enabledBox->setChecked(cheat.enabled);
        connect(enabledBox, &QCheckBox::toggled, [this, i](bool checked) {
            if (i < m_cheats.count()) {
                CheatEntry& cheat = m_cheats[i];
                cheat.enabled = checked;

                if (checked) {
                    if (!cheat.cheatSet) {
                        applyCheat(cheat);
                    }
                    m_logOutput->append(QString("Activated cheat: %1").arg(cheat.description));
                } else {
                    if (cheat.cheatSet) {
                        removeCheat(cheat);
                    }
                    m_logOutput->append(QString("Deactivated cheat: %1").arg(cheat.description));
                }
            }
        });
        m_cheatTable->setCellWidget(i, 0, enabledBox);

        // Address
        QTableWidgetItem *addressItem = new QTableWidgetItem(cheat.address);
        addressItem->setFlags(addressItem->flags() & ~Qt::ItemIsEditable);
        addressItem->setBackground(QColor(240, 240, 240));
        m_cheatTable->setItem(i, 1, addressItem);

        // Value 
        QTableWidgetItem *valueItem = new QTableWidgetItem(cheat.value);
        m_cheatTable->setItem(i, 2, valueItem);

        // Description 
        QTableWidgetItem *descItem = new QTableWidgetItem(cheat.description);
        m_cheatTable->setItem(i, 3, descItem);

        // Frozen
        QCheckBox *frozenBox = new QCheckBox;
        frozenBox->setChecked(cheat.frozen);
        connect(frozenBox, &QCheckBox::toggled, [this, i](bool checked) {
            if (i < m_cheats.count()) {
                CheatEntry& cheat = m_cheats[i];
                cheat.frozen = checked;

                if (checked) {
                    freezeCheat(cheat);
                } else {
                    unfreezeCheat(cheat);
                }

                updateCheatTable();
            }
        });
        m_cheatTable->setCellWidget(i, 4, frozenBox);

        if (cheat.frozen) {
            for (int j = 1; j < 4; j++) {
                if (j != 1) {
                    m_cheatTable->item(i, j)->setBackground(QColor(200, 200, 255));
                }
            }
        }
        if (!cheat.enabled) {
            for (int j = 1; j < 4; j++) {
                if (j == 1) continue;
                m_cheatTable->item(i, j)->setBackground(QColor(220, 220, 220));
            }
        }
    }

    m_cheatTable->blockSignals(false);
}


void CheatEngine::initCheatDevice() {
	if (!m_controller || !m_controller->thread()) {
		m_logOutput->append("Error: No active core controller");
		return;
	}

	mCore* core = m_controller->thread()->core;
	if (!core) {
		m_logOutput->append("Error: No active core");
		return;
	}

	m_cheatDevice = core->cheatDevice(core);
	if (!m_cheatDevice) {
		m_logOutput->append("Error: Core does not support cheats");
		return;
	}

	m_logOutput->append("Cheat device initialized successfully");
}

void CheatEngine::applyCheat(CheatEntry& cheat) {
	if (!m_cheatDevice || !m_controller || !m_controller->thread()) {
		m_logOutput->append("Error: Cheat device not available");
		return;
	}

	mCore* core = m_controller->thread()->core;
	if (!core) {
		m_logOutput->append("Error: No active core");
		return;
	}

	uint32_t address = parseAddress(cheat.address);
	int32_t value = parseValue(cheat.value, cheat.type);
	int width = getWidthFromType(cheat.type);

	if (address == 0 && cheat.address != "0x0" && cheat.address != "0") {
		m_logOutput->append(QString("Error: Invalid address: %1").arg(cheat.address));
		return;
	}

	cheat.cheatSet = m_cheatDevice->createSet(m_cheatDevice, cheat.description.toUtf8().constData());
	if (!cheat.cheatSet) {
		m_logOutput->append("Error: Failed to create cheat set");
		return;
	}

	QString cheatLine = QString("%1:%2").arg(address, 8, 16, QChar('0')).arg(value, width * 2, 16, QChar('0'));

	if (!mCheatAddLine(cheat.cheatSet, cheatLine.toUtf8().constData(), 0)) {
		m_logOutput->append("Error: Failed to add cheat line");
		return;
	}

	mCheatAddSet(m_cheatDevice, cheat.cheatSet);
	mCheatRefresh(m_cheatDevice, cheat.cheatSet);
	m_logOutput->append(QString("Applied cheat: %1 = %2").arg(cheat.address, cheat.value));
}

void CheatEngine::removeCheat(CheatEntry& cheat) {
	if (!m_cheatDevice || !cheat.cheatSet) {
		return;
	}

	mCheatRemoveSet(m_cheatDevice, cheat.cheatSet);
	cheat.cheatSet = nullptr;

	m_logOutput->append(QString("Removed cheat: %1").arg(cheat.description));
}

void CheatEngine::freezeCheat(CheatEntry& cheat) {
	m_logOutput->append(QString("Freezing cheat: %1").arg(cheat.description));
}

void CheatEngine::unfreezeCheat(CheatEntry& cheat) {
	m_logOutput->append(QString("Unfreezing cheat: %1").arg(cheat.description));
}

void CheatEngine::directMemoryFreeze(const CheatEntry& cheat) {
	if (!m_controller || !m_controller->thread()) {
		return;
	}

	mCore* core = m_controller->thread()->core;
	if (!core) {
		return;
	}

	uint32_t address = parseAddress(cheat.address);
	int32_t targetValue = parseValue(cheat.value, cheat.type);
	int width = getWidthFromType(cheat.type);

	if (address == 0 && cheat.address != "0x0" && cheat.address != "0") {
		return;
	}

	int32_t currentValue = 0;
	switch (width) {
	case 1:
		currentValue = core->busRead8(core, address);
		break;
	case 2:
		currentValue = core->busRead16(core, address);
		break;
	case 4:
		currentValue = core->busRead32(core, address);
		break;
	}

	if (currentValue != targetValue) {
		FreezeStats& stats = m_freezeStats[cheat.address];
		uint32_t currentFrame = core->frameCounter(core);

		stats.changeCount++;
		stats.lastChangeFrame = currentFrame;

		const struct mCoreMemoryBlock* block = mCoreGetMemoryBlockInfo(core, address);

		if (block && (block->flags & mCORE_MEMORY_WRITE)) {
			switch (width) {
			case 1:
				core->rawWrite8(core, address, block->id, targetValue);
				break;
			case 2:
				core->rawWrite16(core, address, block->id, targetValue);
				break;
			case 4:
				core->rawWrite32(core, address, block->id, targetValue);
				break;
			}
		}

		switch (width) {
		case 1:
			core->busWrite8(core, address, targetValue);
			break;
		case 2:
			core->busWrite16(core, address, targetValue);
			break;
		case 4:
			core->busWrite32(core, address, targetValue);
			break;
		}

		if ((currentFrame - stats.lastChangeFrame) > 60 || stats.changeCount == 1) {
			m_logOutput->append(
			    QString("Frame-sync freeze: %1 (%2 -> %3)").arg(cheat.address).arg(currentValue).arg(targetValue));
		}

		stats.lastKnownValue = targetValue;
	}
}

void CheatEngine::onValueChanged() { }

void CheatEngine::setupFrameCallback() {
	if (!m_controller || !m_controller->thread()) {
		m_logOutput->append("Error: Cannot setup frame callback - no core controller");
		return;
	}

	mCore* core = m_controller->thread()->core;
	if (!core) {
		m_logOutput->append("Error: Cannot setup frame callback - no core");
		return;
	}

	m_coreCallbacks.videoFrameStarted = nullptr;
	m_coreCallbacks.videoFrameEnded = &CheatEngine::coreFrameCallback;
	m_coreCallbacks.coreCrashed = nullptr;
	m_coreCallbacks.sleep = nullptr;
	m_coreCallbacks.shutdown = nullptr;
	m_coreCallbacks.keysRead = nullptr;
	m_coreCallbacks.savedataUpdated = nullptr;
	m_coreCallbacks.alarm = nullptr;
	m_coreCallbacks.context = this;

	core->addCoreCallbacks(core, &m_coreCallbacks);
	m_logOutput->append("Frame callback registered - freeze system will sync with game frames");
}

void CheatEngine::removeFrameCallback() {
	if (!m_controller || !m_controller->thread()) {
		return;
	}

	mCore* core = m_controller->thread()->core;
	if (!core) {
		return;
	}

	core->clearCoreCallbacks(core);
}

void CheatEngine::coreFrameCallback(void* context) {
	CheatEngine* engine = static_cast<CheatEngine*>(context);
	if (engine) {
		engine->onFrameComplete();
	}
}

void CheatEngine::onFrameComplete() {
	if (!m_controller || !m_controller->thread()) {
		return;
	}

	mCore* core = m_controller->thread()->core;
	if (!core) {
		return;
	}

	for (const auto& cheat : m_cheats) {
		if (cheat.frozen && cheat.enabled) {
			directMemoryFreeze(cheat);
		}
	}
}

int CheatEngine::getWidthFromType(const QString& type) {
	if (type == "Byte") {
		return 1;
	} else if (type == "Word") {
		return 2;
	}
	return 1;
}

uint32_t CheatEngine::parseAddress(const QString& address) {
	bool ok;
	uint32_t addr = address.toUInt(&ok, 16);
	if (!ok) {
		// Try without 0x prefix
		addr = address.mid(2).toUInt(&ok, 16);
	}
	return ok ? addr : 0;
}

int32_t CheatEngine::parseValue(const QString& value, const QString& type) {
	bool ok;
	int32_t val;

	if (value.startsWith("0x", Qt::CaseInsensitive)) {
		val = value.toInt(&ok, 16);
	} else {
		val = value.toInt(&ok, 10);
	}

	if (!ok) {
		return 0;
	}

	if (type == "Byte") {
		return val & 0xFF;
	} else if (type == "Word") {
		return val & 0xFFFF;
	}

	return val;
}

void CheatEngine::setupHotkeys() {
	QShortcut* toggleAll = new QShortcut(QKeySequence("Ctrl+T"), this);
	QShortcut* exportShortcut = new QShortcut(QKeySequence("Ctrl+E"), this);

	connect(toggleAll, &QShortcut::activated, this, &CheatEngine::toggleAllCheats);
	connect(exportShortcut, &QShortcut::activated, this, &CheatEngine::onExportMap);

	for (int i = 0; i < 5; i++) {
		QShortcut* quickToggle = new QShortcut(QKeySequence(QString("Shift+%1").arg(i + 1)), this);
		connect(quickToggle, &QShortcut::activated, [this, i]() {
			if (i < m_cheats.count()) {
				CheatEntry& cheat = m_cheats[i];
				cheat.enabled = !cheat.enabled;

				if (cheat.enabled) {
					applyCheat(cheat);
				} else {
					removeCheat(cheat);
				}
				updateCheatTable();
				m_logOutput->append(QString("Shift+%1 toggled: %2").arg(i + 1).arg(cheat.description));
			}
		});
	}

	m_logOutput->append("Hotkeys enabled: Ctrl+T (toggle all), Shift+1-5 (quick toggle)");
	m_logOutput->append("Hotkey enabled: Ctrl+E (export cheats)");
}

void CheatEngine::toggleAllCheats() {
	bool anyEnabled = false;
	for (const auto& cheat : m_cheats) {
		if (cheat.enabled) {
			anyEnabled = true;
			break;
		}
	}

	bool newState = !anyEnabled;

	for (auto& cheat : m_cheats) {
		cheat.enabled = newState;
		if (newState) {
			applyCheat(cheat);
		} else {
			removeCheat(cheat);
		}
	}

	updateCheatTable();
	m_logOutput->append(QString("All cheats %1").arg(newState ? "enabled" : "disabled"));
}