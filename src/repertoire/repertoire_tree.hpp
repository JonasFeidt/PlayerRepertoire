
#pragma once

#include "../core/chessGame.hpp"

#include <cstddef>
#include <string>
#include <vector>

struct RepertoireNode {
    std::string moveUci;
    std::string moveSan;

    std::size_t gameCount = 0;

    std::vector<RepertoireNode> children;
};

struct RepertoireTree {
    // Different starting positions must have separate trees.
    // Empty means the standard starting position.
    std::string startingFen;

    RepertoireNode root;
};

class RepertoireBuilder {
public:
    static std::vector<RepertoireTree> build(
        const std::vector<ChessGame>& games,
        int maxFullMove = 20
    );
};

class RepertoireNotation {
public:
    static bool convertToSan(
        RepertoireTree& tree,
        std::string& error
    );

    static std::string toText(const RepertoireTree& tree);
};
