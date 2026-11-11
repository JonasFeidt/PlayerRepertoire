#include "database/database.hpp"
#include "pgn/import.hpp"

#include <filesystem>
#include <iostream>

int main() {
    Database database;

    if (!database.open("test_collection.db")) {
        std::cerr << "Database initialization failed.\n";
        return 1;
    }

    PGNImporter importer;

    const auto result = importer.importFile("tests/sample.pgn");

    if (!result.success) {
        std::cerr << "PGN import failed.\n";

        for (const auto& error : result.errors) {
            std::cerr << error << '\n';
        }

        return 1;
    }

    if (!database.beginTransaction()) {
        std::cerr << "Could not start transaction.\n";
        return 1;
    }

    const int sourceId =
        database.insertImportSource("tests/sample.pgn");

    if (sourceId < 0) {
        database.rollbackTransaction();
        std::cerr << "Could not create import source.\n";
        return 1;
    }

    int inserted = 0;

    for (auto game : result.games) {
        game.importSourceId = sourceId;

        if (!database.insertGame(game)) {
            database.rollbackTransaction();
            std::cerr << "Game insertion failed.\n";
            return 1;
        }

        ++inserted;
    }

    if (!database.updateImportSourceGameCount(sourceId, inserted) ||
        !database.commitTransaction()) {
        database.rollbackTransaction();
        std::cerr << "Could not commit database transaction.\n";
        return 1;
    }

    std::cout << "Games saved: " << inserted << '\n';
    std::cout << "Database: test_collection.db\n";

    database.close();

    return 0;
}