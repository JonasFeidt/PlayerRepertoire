#pragma once

#include <optional>
#include <string>

struct GameMetadata {
    std::string event;
    std::string site;
    std::string date;
    std::string round;

    std::string white;
    std::string black;
    std::string result;

    std::optional<int> whiteElo;
    std::optional<int> blackElo;

    std::string eco;
    std::string opening;
    std::string timeControl;
};