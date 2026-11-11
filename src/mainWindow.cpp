
#include "mainWindow.hpp"

#include "database/database.hpp"
#include "repertoire/repertoire_tree.hpp"
#include "repertoire/pgn_exporter.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include "pgn/import.hpp"

#include <optional>
#include <string>

namespace {

void addNodeToView(
    QTreeWidgetItem* parent,
    const RepertoireNode& node
) {
    const std::string& notation =
        node.moveSan.empty() ? node.moveUci : node.moveSan;

    auto* item = new QTreeWidgetItem(
        parent,
        {
            QString::fromStdString(notation),
            QString::number(node.gameCount)
        }
    );

    for (const auto& child : node.children) {
        addNodeToView(item, child);
    }
}

QString startingPositionName(
    const RepertoireTree& tree,
    std::size_t index
) {
    if (tree.startingFen.empty()) {
        return QString("Starting position %1 (standard)")
            .arg(index + 1);
    }

    return QString("Starting position %1 (custom FEN)")
        .arg(index + 1);
}

} // namespace

MainWindow::MainWindow(Database& database, QWidget* parent)
    : QMainWindow(parent),
      m_database(database) {

    setWindowTitle("Chess Repertoire");
    resize(1000, 700);

    auto* centralWidget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(centralWidget);

    // Input section
    auto* inputGroup = new QGroupBox("Repertoire settings");
    auto* inputLayout = new QFormLayout(inputGroup);

    m_playerInput = new QLineEdit();
    m_playerInput->setPlaceholderText("e.g. Carlsen, Magnus");

    m_colorInput = new QComboBox();
    m_colorInput->addItem("White", 0);
    m_colorInput->addItem("Black", 1);

    m_useDateFilter = new QCheckBox("Filter by date");

    m_yearInput->setEnabled(false);
    m_monthInput->setEnabled(false);

    connect(
    m_useDateFilter,
    &QCheckBox::toggled,
    m_yearInput,
    &QSpinBox::setEnabled
    );

    connect(
    m_useDateFilter,
    &QCheckBox::toggled,
    m_monthInput,
    &QComboBox::setEnabled
    );

    auto* dateLayout = new QHBoxLayout();
    dateLayout->addWidget(m_useDateFilter);
    dateLayout->addWidget(m_yearInput);
    dateLayout->addWidget(m_monthInput);

    m_maxMovesInput = new QSpinBox();
    m_maxMovesInput->setRange(1, 100);
    m_maxMovesInput->setValue(20);
    m_maxMovesInput->setSuffix(" full moves");

    inputLayout->addRow("Player name:", m_playerInput);
    inputLayout->addRow("Player's color:", m_colorInput);
    inputLayout->addRow("Date:", dateLayout);
    inputLayout->addRow("Maximum length:", m_maxMovesInput);

    mainLayout->addWidget(inputGroup);
    auto* importButton = new QPushButton("Import PGN file");

    connect(
    importButton,
    &QPushButton::clicked,
    this,
    &MainWindow::importPGN
    );

mainLayout->addWidget(importButton);

    // Generate button
    auto* generateButton = new QPushButton("Generate repertoire");

    connect(
        generateButton,
        &QPushButton::clicked,
        this,
        &MainWindow::generateRepertoire
    );

    mainLayout->addWidget(generateButton);

    // Starting position selector
    auto* positionLayout = new QHBoxLayout();

    positionLayout->addWidget(
        new QLabel("Starting position:")
    );

    m_startingPositionInput = new QComboBox();

    connect(
        m_startingPositionInput,
        &QComboBox::currentIndexChanged,
        this,
        [this](int) {
            updateRepertoireView();
        }
    );

    positionLayout->addWidget(m_startingPositionInput, 1);
    mainLayout->addLayout(positionLayout);

    // Repertoire tree
    m_repertoireView = new QTreeWidget();
    m_repertoireView->setColumnCount(2);
    m_repertoireView->setHeaderLabels(
        {"Move / Variation", "Games"}
    );
    m_repertoireView->setColumnWidth(0, 300);

    mainLayout->addWidget(m_repertoireView, 1);

    // Status and export
    auto* bottomLayout = new QHBoxLayout();

    m_statusLabel = new QLabel("Enter a player to generate a repertoire.");
    bottomLayout->addWidget(m_statusLabel, 1);

    auto* exportButton = new QPushButton("Export PGN");

    connect(
        exportButton,
        &QPushButton::clicked,
        this,
        &MainWindow::exportRepertoire
    );

    bottomLayout->addWidget(exportButton);
    mainLayout->addLayout(bottomLayout);

    setCentralWidget(centralWidget);
}

void MainWindow::generateRepertoire() {
    const std::string player =
        m_playerInput->text().trimmed().toStdString();

    if (player.empty()) {
        QMessageBox::warning(
            this,
            "Missing player",
            "Please enter a player name."
        );
        return;
    }

    // Preserve the PGN name format, e.g. "Carlsen, Magnus".
    const Database::PlayerColor color =
        m_colorInput->currentData().toInt() == 0
            ? Database::PlayerColor::White
            : Database::PlayerColor::Black;

    std::optional<std::string> date;

    if (m_useDateFilter->isChecked()) {
    const int year = m_yearInput->value();
    const int month = m_monthInput->currentData().toInt();

    QString startDate;

    if (month == 0) {
        startDate = QString("%1.01.01").arg(year, 4, 10, QChar('0'));
    } else {
        startDate = QString("%1.%2.01")
                        .arg(year, 4, 10, QChar('0'))
                        .arg(month, 2, 10, QChar('0'));
    }

    date = startDate.toStdString();
    }

    const auto games =
        m_database.searchGames(player, color, date);

    if (games.empty()) {
        m_trees.clear();
        m_startingPositionInput->clear();
        m_repertoireView->clear();

        m_statusLabel->setText("No matching games found.");

        QMessageBox::information(
            this,
            "No games found",
            "No games were found for this player and filter."
        );
        return;
    }

    m_trees = RepertoireBuilder::build(
        games,
        m_maxMovesInput->value()
    );

    // Convert every tree from UCI to SAN.
    for (auto& tree : m_trees) {
        std::string error;

        if (!RepertoireNotation::convertToSan(tree, error)) {
            m_trees.clear();

            QMessageBox::critical(
                this,
                "Notation conversion failed",
                QString::fromStdString(error)
            );

            return;
        }
    }

    m_startingPositionInput->clear();

    for (std::size_t i = 0; i < m_trees.size(); ++i) {
        m_startingPositionInput->addItem(
            startingPositionName(m_trees[i], i)
        );
    }

    m_statusLabel->setText(
        QString("Found %1 games. Generated %2 repertoire tree(s).")
            .arg(games.size())
            .arg(m_trees.size())
    );

    m_startingPositionInput->setCurrentIndex(0);
    updateRepertoireView();
}

void MainWindow::updateRepertoireView() {
    m_repertoireView->clear();

    const int index = m_startingPositionInput->currentIndex();

    if (index < 0 ||
        index >= static_cast<int>(m_trees.size())) {
        return;
    }

    const RepertoireTree& tree =
        m_trees[static_cast<std::size_t>(index)];

    auto* rootItem = new QTreeWidgetItem(
        m_repertoireView,
        {
            "Starting position",
            QString::number(tree.root.gameCount)
        }
    );

    for (const auto& child : tree.root.children) {
        addNodeToView(rootItem, child);
    }

    rootItem->setExpanded(true);
    m_repertoireView->expandToDepth(1);
}

void MainWindow::exportRepertoire() {
    const int index = m_startingPositionInput->currentIndex();

    if (index < 0 ||
        index >= static_cast<int>(m_trees.size())) {
        QMessageBox::information(
            this,
            "Nothing to export",
            "Generate a repertoire first."
        );
        return;
    }

    const std::string player =
        m_playerInput->text().trimmed().toStdString();

    const bool playerIsWhite =
        m_colorInput->currentData().toInt() == 0;

    PGNExportOptions options;
    options.event = "Generated repertoire";
    options.white = playerIsWhite ? player : "?";
    options.black = playerIsWhite ? "?" : player;
    options.result = "*";

    const QString filename = QFileDialog::getSaveFileName(
        this,
        "Export repertoire",
        playerIsWhite
            ? "repertoire-white.pgn"
            : "repertoire-black.pgn",
        "PGN files (*.pgn)"
    );

    if (filename.isEmpty()) {
        return;
    }

    std::string error;

    const bool success = PGNExporter::writeToFile(
        m_trees[static_cast<std::size_t>(index)],
        options,
        filename.toStdString(),
        error
    );

    if (!success) {
        QMessageBox::critical(
            this,
            "Export failed",
            QString::fromStdString(error)
        );
        return;
    }

    QMessageBox::information(
        this,
        "Export complete",
        "The repertoire was exported successfully."
    );
}


void MainWindow::importPGN() {
    const QString filename = QFileDialog::getOpenFileName(
        this,
        "Import PGN file",
        QString(),
        "PGN files (*.pgn);;All files (*)"
    );

    if (filename.isEmpty()) {
        return;
    }

    PGNImporter importer;

    const PGNImportResult result =
        importer.importFile(filename.toStdString());

    if (!result.success) {
        QString message = "Could not import PGN file.";

        for (const auto& error : result.errors) {
            message += "\n" + QString::fromStdString(error);
        }

        QMessageBox::critical(this, "Import failed", message);
        return;
    }

    if (result.games.empty()) {
        QMessageBox::information(
            this,
            "Empty PGN",
            "The file did not contain any games."
        );
        return;
    }

    const int sourceId =
        m_database.insertImportSource(
            filename.toStdString()
        );

    if (sourceId < 0) {
        QMessageBox::critical(
            this,
            "Import failed",
            "Could not create the import source in the database."
        );
        return;
    }

    if (!m_database.beginTransaction()) {
        QMessageBox::critical(
            this,
            "Import failed",
            "Could not start the database transaction."
        );
        return;
    }

    for (auto game : result.games) {
        game.importSourceId = sourceId;

        if (!m_database.insertGame(game)) {
            m_database.rollbackTransaction();

            QMessageBox::critical(
                this,
                "Import failed",
                "Could not insert a game into the database."
            );
            return;
        }
    }

    if (!m_database.updateImportSourceGameCount(
            sourceId,
            static_cast<int>(result.games.size()))) {
        m_database.rollbackTransaction();

        QMessageBox::critical(
            this,
            "Import failed",
            "Could not update the import source."
        );
        return;
    }

    if (!m_database.commitTransaction()) {
        m_database.rollbackTransaction();

        QMessageBox::critical(
            this,
            "Import failed",
            "Could not commit the database transaction."
        );
        return;
    }

    m_statusLabel->setText(
        QString("Imported %1 games.")
            .arg(result.games.size())
    );

    QString message =
        QString("Successfully imported %1 games.")
            .arg(result.games.size());

    if (!result.errors.empty()) {
        message += "\n\nSome import warnings occurred:";
        for (const auto& error : result.errors) {
            message += "\n" + QString::fromStdString(error);
        }
    }

    QMessageBox::information(this, "Import complete", message);
}

