FetchContent_Declare(
        godot-cpp
        GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
        GIT_TAG        master # Attention: master peut casser. Préfère un tag stable (ex: 4.3) si possible.
        GIT_SHALLOW    TRUE   # Télécharge plus vite (sans tout l'historique git)
)

FetchContent_MakeAvailable(godot-cpp)