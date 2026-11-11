#pragma once

#include "gameMetadata.hpp"

#include <string>
#include <vector>

struct ChessGame {
    int id = -1;
    int importSourceId = -1;

    GameMetadata metadata;

    // Empty means the standard chess starting position.
    // Otherwise, this contains the starting FEN.
    std::string startingFen;

    // Mainline moves stored in UCI format.
    std::vector<std::string> movesUci;

    // Original PGN, including comments and variations.
    std::string originalPgn;
};