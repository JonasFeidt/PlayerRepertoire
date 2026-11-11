#pragma once

#include "../core/chessGame.hpp"

#include <filesystem>
#include <string>
#include <vector>

struct PGNImportResult {
    std::vector<ChessGame> games;
    std::vector<std::string> errors;

    bool success = false;
};

class PGNImporter {
public:
    PGNImportResult importFile(
        const std::filesystem::path& path
    );
};