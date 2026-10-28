#pragma once
#include <windows.h>
#include <vector>
#include "Gizmo.h"

// Идентификаторы для контекстного меню Windows
#define ID_MENU_DELETE  1001
#define ID_MENU_COPY    1002
#define ID_MENU_CUT     1003
#define ID_MENU_PASTE   1004
#define ID_MENU_LOCK    1005
#define ID_MENU_IMPORT_OBJ 1006

// Функция управления сценой
void ExecuteSceneAction(
    int actionID,
    std::vector<EditableCube>& cubes,
    int& selectedIdx,
    EditableCube& clipboard,
    bool& hasData,
    float spawnX, float spawnY, float spawnZ
);

// Функция выбора файлов (возвращает системный указатель на строку)
const wchar_t* Open3DFileDialog(HWND hwnd);
// ДОБАВИТЬ В САМЫЙ КОНЕЦ ФАЙЛА SceneActions.h:
bool LoadOBJModel(const wchar_t* filename, EditableCube& outModel);
