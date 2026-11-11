
#include "database/database.hpp"
#include "pgn/import.hpp"
#include "repertoire/repertoire_tree.hpp"
#include "repertoire/pgn_exporter.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 5 || argc > 6) {
        std::cerr
            << "Usage: " << argv[0]
            << " <file.pgn> <player> <white|black> <output.pgn> [date]\n";
        return 1;
    }

    const std::filesystem::path pgnPath = argv[1];
    const std::string player = argv[2];
    const std::string colorArg = argv[3];
    const std::filesystem::path outputPath = argv[4];

    Database::PlayerColor color;
    if (colorArg == "white") {
        color = Database::PlayerColor::White;
    } else if (colorArg == "black") {
        color = Database::PlayerColor::Black;
    } else {
        std::cerr << "Color must be 'white' or 'black'.\n";
        return 1;
    }

    std::optional<std::string> date;
    if (argc == 6) {
        date = argv[5];
    }

    // Use an in-memory database so each run starts clean.
    Database database;
    if (!database.open(":memory:")) {
        std::cerr << "Could not open database.\n";
        return 1;
    }

    // Import the PGN file.
    PGNImporter importer;
    PGNImportResult importResult = importer.importFile(pgnPath);

    for (const auto& error : importResult.errors) {
        std::cerr << "Import warning/error: " << error << '\n';
    }

    if (!importResult.success) {
        std::cerr << "PGN import failed.\n";
        return 1;
    }

    std::cout << "Imported " << importResult.games.size()
              << " games.\n";

    // Store imported games in the database.
    const int sourceId =
        database.insertImportSource(pgnPath.filename().string());

    if (sourceId < 0) {
        std::cerr << "Could not create import source.\n";
        return 1;
    }

    if (!database.beginTransaction()) {
        std::cerr << "Could not begin database transaction.\n";
        return 1;
    }

    for (auto& game : importResult.games) {
        game.importSourceId = sourceId;

        if (!database.insertGame(game)) {
            std::cerr << "Could not insert a game.\n";
            database.rollbackTransaction();
            return 1;
        }
    }

    database.updateImportSourceGameCount(
        sourceId,
        static_cast<int>(importResult.games.size())
    );

    if (!database.commitTransaction()) {
        std::cerr << "Could not commit database transaction.\n";
        database.rollbackTransaction();
        return 1;
    }

    // Find games where the selected player played the selected color.
    const std::vector<ChessGame> games =
        database.searchGames(player, color, date);

    std::cout << "Found " << games.size()
              << " games for " << player << ".\n";

    if (games.empty()) {
        std::cerr << "No matching games found.\n";
        return 1;
    }

    // Build repertoire trees, limited to 20 full moves.
    auto trees = RepertoireBuilder::build(games, 20);

    std::cout << "Generated " << trees.size()
              << " repertoire tree(s).\n";

    PGNExportOptions options;
    options.event = "Generated repertoire";
    options.white = color == Database::PlayerColor::White ? player : "?";
    options.black = color == Database::PlayerColor::Black ? player : "?";
    options.result = "*";

    // Convert and export each starting-position tree.
    for (std::size_t i = 0; i < trees.size(); ++i) {
        std::string error;

        if (!RepertoireNotation::convertToSan(trees[i], error)) {
            std::cerr << "SAN conversion failed: " << error << '\n';
            return 1;
        }

        std::filesystem::path filePath = outputPath;

        // Multiple starting positions need separate PGN files.
        if (trees.size() > 1) {
            filePath = outputPath.parent_path() /
                (outputPath.stem().string() +
                 "_start_" + std::to_string(i + 1) +
                 outputPath.extension().string());
        }

        if (!PGNExporter::writeToFile(
                trees[i], options, filePath.string(), error)) {
            std::cerr << "Export failed: " << error << '\n';
            return 1;
        }

        std::cout << "Exported: " << filePath << '\n';
    }

    return 0;
}
