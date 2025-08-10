#ifndef CHEATENGINE_H
#define CHEATENGINE_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QTableWidgetItem>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QLabel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QHeaderView>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include "CoreController.h"

extern "C" {
#include <mgba/core/cheats.h>
#include <mgba/core/core.h>
} 



class CheatEngine : public QMainWindow
{
    Q_OBJECT

public:
    CheatEngine(std::shared_ptr<QGBA::CoreController> controller, QWidget* parent = nullptr);
    ~CheatEngine();

private slots:
    void onAddCheat();
    void onRemoveCheat();
    void onImportMap();
    void onValueChanged();
    void onCheatTableSelectionChanged();
    void onFreezeTimer(); // For maintaining frozen values

private:
    void setupUI();
    void setupCheatManager();
    void updateCheatTable();
    
    // mGBA integration functions (using basic types)
    void initCheatDevice();
    int getWidthFromType(const QString& type);
    uint32_t parseAddress(const QString& address);
    int32_t parseValue(const QString& value, const QString& type);

    // Main layout components
    QWidget *m_centralWidget;
    QSplitter *m_mainSplitter;

    // Memory viewer - REMOVED (functionality exists elsewhere)

    // Memory search - REMOVED (not needed for direct cheat management)

    // Cheat manager
    QGroupBox *m_cheatGroup;
    QTableWidget *m_cheatTable;
    QPushButton *m_addCheatButton;
    QPushButton *m_removeCheatButton;
    QPushButton *m_importMapButton;

    // Cheat editor
    QGroupBox *m_editorGroup;
    QLineEdit *m_cheatAddress;
    QLineEdit *m_cheatValue;
    QLineEdit *m_cheatDescription;
    QComboBox *m_cheatType;
    QCheckBox *m_freezeCheckbox;

    // Status and info
    QTextEdit *m_logOutput;
    QLabel *m_statusLabel;

    // Data structures
    struct CheatEntry {
        QString address;
        QString value;
        QString description;
        QString type;
        bool frozen;
        bool enabled;
        struct mCheatSet* cheatSet; // Pointer to the actual mGBA cheat set
    };

    // CheatEntry-dependent functions (declared after struct definition)
    void applyCheat(CheatEntry& cheat);
    void removeCheat(CheatEntry& cheat);
    void freezeCheat(CheatEntry& cheat);
    void unfreezeCheat(CheatEntry& cheat);

    QList<CheatEntry> m_cheats;
    std::shared_ptr<QGBA::CoreController> m_controller;
    
    // mGBA cheat system integration
    struct mCheatDevice* m_cheatDevice;
    QTimer* m_freezeTimer;

};

#endif // CHEATENGINE_H
