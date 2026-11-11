#include "pgn/import.hpp"

#include <iostream>

int main() {
    PGNImporter importer;

    const auto result = importer.importFile(
        "./tests/sample.pgn"
    );

    std::cout << "Import successful: "
              << std::boolalpha << result.success << '\n';

    std::cout << "Games imported: "
              << result.games.size() << '\n';

    std::cout << "Errors: "
              << result.errors.size() << '\n';

    for (const auto& error : result.errors) {
        std::cout << "Error: " << error << '\n';
    }

    for (const auto& game : result.games) {
        std::cout << "\n"
                  << game.metadata.white
                  << " vs "
                  << game.metadata.black << '\n';

        std::cout << "Moves: ";

        for (const auto& move : game.movesUci) {
            std::cout << move << ' ';
        }

        std::cout << '\n';
    }

    // Basic automated checks.
    if (!result.success || result.games.size() != 2) {
        return 1;
    }

    const auto& firstGame = result.games[0];

    if (firstGame.metadata.white != "Alice" ||
        firstGame.metadata.black != "Bob" ||
        firstGame.movesUci.size() != 4 ||
        firstGame.movesUci[0] != "e2e4" ||
        firstGame.movesUci[1] != "e7e5") {
        std::cerr << "First game test failed.\n";
        return 1;
    }

    std::cout << "\nAll tests passed!\n";
    return 0;
}