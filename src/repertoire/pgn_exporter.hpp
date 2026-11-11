
#pragma once

#include "repertoire_tree.hpp"

#include <string>

struct PGNExportOptions {
    std::string event = "Generated repertoire";
    std::string site = "?";
    std::string date = "????.??.??";
    std::string round = "?";

    std::string white = "?";
    std::string black = "?";

    std::string result = "*";
};

class PGNExporter {
public:
    static std::string toPgn(
        const RepertoireTree& tree,
        const PGNExportOptions& options,
        std::string& error
    );

    static bool writeToFile(
        const RepertoireTree& tree,
        const PGNExportOptions& options,
        const std::string& filename,
        std::string& error
    );
};
