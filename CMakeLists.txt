cmake_minimum_required(VERSION 4.1)
project(GodotNetworking LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- CONFIGURATION ---
# Passage à la version 4.6 comme demandé
set(GODOT_VERSION "4.6-stable")
set(GODOT_CACHE_DIR "${CMAKE_SOURCE_DIR}/.godot_bin")
set(GODOT_PROJECT_DIR "${CMAKE_SOURCE_DIR}/game")

# --- DÉTECTION DU CHEMIN EXÉCUTABLE (Version corrigée) ---
if(APPLE)
    set(GODOT_EXECUTABLE "${GODOT_CACHE_DIR}/Godot.app/Contents/MacOS/Godot")
    set(GODOT_OUTPUT_DIR "${GODOT_PROJECT_DIR}/bin/macos/arm64")
elseif(WIN32)
    set(GODOT_EXECUTABLE "${GODOT_CACHE_DIR}/Godot_v${GODOT_VERSION}_win64.exe")
    set(GODOT_OUTPUT_DIR "${GODOT_PROJECT_DIR}/bin/windows")
else()
    set(GODOT_EXECUTABLE "${GODOT_CACHE_DIR}/Godot_v${GODOT_VERSION}_linux.x86_64")
    set(GODOT_OUTPUT_DIR "${GODOT_PROJECT_DIR}/bin/linux")
endif()

# --- SOUS-PROJETS ---
add_subdirectory(external)
add_subdirectory(src)

# --- TARGETS UTILITAIRES ---

# 1. SETUP : Télécharge Godot
add_custom_target(setup
COMMAND ${CMAKE_COMMAND}
-DGODOT_VERSION=${GODOT_VERSION}
-DGODOT_CACHE_DIR="${GODOT_CACHE_DIR}"
-P "${CMAKE_SOURCE_DIR}/cmake/DownloadGodot.cmake"
COMMENT "Vérification de l'installation de Godot ${GODOT_VERSION}..."
)

# 2. EDITOR : Lance l'éditeur
add_custom_target(editor
COMMAND ${GODOT_EXECUTABLE} -e --path "${GODOT_PROJECT_DIR}"
# On dépend de TA librairie pour qu'elle compile avant
DEPENDS setup GodoNet_client
WORKING_DIRECTORY "${GODOT_PROJECT_DIR}"
COMMENT "Lancement de l'éditeur Godot..."
USES_TERMINAL
)

# 3. PLAY : Lance le jeu
add_custom_target(play
COMMAND ${GODOT_EXECUTABLE} --path "${GODOT_PROJECT_DIR}"
DEPENDS setup GodoNet_client
WORKING_DIRECTORY "${GODOT_PROJECT_DIR}"
COMMENT "Lancement du jeu..."
USES_TERMINAL
)