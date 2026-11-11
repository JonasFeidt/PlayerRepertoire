
cmake_minimum_required(VERSION 3.20)

project(ChessRepertoire
    VERSION 1.0
    LANGUAGES CXX
)

# --------------------------------------------------
# C++ configuration
# --------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

# --------------------------------------------------
# Dependencies
# --------------------------------------------------

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(SQLite3 REQUIRED)

include(FetchContent)

FetchContent_Declare(
    chess_library
    GIT_REPOSITORY https://github.com/Disservin/chess-library.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(chess_library)

# --------------------------------------------------
# Shared target configuration
# --------------------------------------------------

function(configure_project_target target)
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${chess_library_SOURCE_DIR}/include
    )

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
endfunction()

# --------------------------------------------------
# Main application
# --------------------------------------------------

add_executable(ChessRepertoire
    src/main.cpp
    src/mainWindow.cpp
    src/mainWindow.hpp
    src/database/database.cpp
    src/pgn/import.cpp
    src/repertoire/repertoire_tree.cpp
    src/repertoire/repertoire_san_notation.cpp
    src/repertoire/pgn_exporter.cpp
)

set_target_properties(ChessRepertoire PROPERTIES
    AUTOMOC ON
)

configure_project_target(ChessRepertoire)

target_link_libraries(ChessRepertoire PRIVATE
    Qt6::Widgets
    SQLite3::SQLite3
)

# --------------------------------------------------
# Tests
# --------------------------------------------------

if(BUILD_TESTING)

    # PGN importer test
    add_executable(PGNImportTest
        tests/pgnImport_test.cpp
        src/pgn/import.cpp
    )

    configure_project_target(PGNImportTest)

    add_test(
        NAME PGNImportTest
        COMMAND PGNImportTest
    )

    set_tests_properties(PGNImportTest PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )


    # Database import test
    add_executable(DatabaseImportTest
        tests/database_import_test.cpp
        src/database/database.cpp
        src/pgn/import.cpp
    )

    configure_project_target(DatabaseImportTest)

    target_link_libraries(DatabaseImportTest PRIVATE
        SQLite3::SQLite3
    )

    add_test(
        NAME DatabaseImportTest
        COMMAND DatabaseImportTest
    )

    set_tests_properties(DatabaseImportTest PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )


    # Full repertoire generation CLI test
    add_executable(RepertoireCLI
        tests/full_test.cpp
        src/database/database.cpp
        src/pgn/import.cpp
        src/repertoire/repertoire_tree.cpp
        src/repertoire/repertoire_san_notation.cpp
        src/repertoire/pgn_exporter.cpp
    )

    configure_project_target(RepertoireCLI)

    target_link_libraries(RepertoireCLI PRIVATE
        SQLite3::SQLite3
    )

endif()
