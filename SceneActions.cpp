#include "SceneActions.h"
#include <commdlg.h>
#include <fstream>
#include <sstream>
// Глобальный статический буфер для хранения пути к файлу
static wchar_t g_SelectedFilePath[MAX_PATH] = { 0 };

void ExecuteSceneAction(
    int actionID,
    std::vector<EditableCube>& cubes,
    int& selectedIdx,
    EditableCube& clipboard,
    bool& hasData,
    float spawnX, float spawnY, float spawnZ
) {
    switch (actionID) {
    case ID_MENU_DELETE:
        if (selectedIdx != -1 && !cubes[selectedIdx].isLocked) {
            cubes[selectedIdx].exists = false;
            cubes[selectedIdx].isSelected = false;
            selectedIdx = -1;
        }
        break;

    case ID_MENU_LOCK:
        if (selectedIdx != -1) {
            cubes[selectedIdx].isLocked = !cubes[selectedIdx].isLocked;
            if (cubes[selectedIdx].isLocked) {
                cubes[selectedIdx].isSelected = false;
                selectedIdx = -1;
            }
        }
        break;

    case ID_MENU_COPY:
        if (selectedIdx != -1 && !cubes[selectedIdx].isLocked) {
            clipboard = cubes[selectedIdx];
            clipboard.isSelected = false;
            hasData = true;
        }
        break;

    case ID_MENU_CUT:
        if (selectedIdx != -1 && !cubes[selectedIdx].isLocked) {
            clipboard = cubes[selectedIdx];
            clipboard.isSelected = false;
            hasData = true;
            cubes[selectedIdx].exists = false;
            selectedIdx = -1;
        }
        break;

    case ID_MENU_PASTE:
        if (hasData) {
            EditableCube pasteClone = clipboard;
            pasteClone.posX = spawnX;
            pasteClone.posY = spawnY;
            pasteClone.posZ = spawnZ;
            pasteClone.isLocked = false;
            pasteClone.isSelected = false;
            cubes.push_back(pasteClone);
        }
        break;
    }
}

// Новая функция проводника Windows без использования std::wstring
const wchar_t* Open3DFileDialog(HWND hwnd) {
    g_SelectedFilePath[0] = L'\0'; // Очищаем буфер

    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = g_SelectedFilePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"3D Wavefront Models (*.obj)\0*.obj\0Все файлы (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        return g_SelectedFilePath;
    }
    return L"";
}
bool LoadOBJModel(const wchar_t* filename, EditableCube& outModel) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    outModel.vertices.clear();
    outModel.faces.clear();
    outModel.isCustomModel = true;

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "v") {
            Vertex3D v;
            ss >> v.x >> v.y >> v.z;
            outModel.vertices.push_back(v);
        }
        else if (type == "f") {
            Face3D f;
            std::string v1, v2, v3;
            ss >> v1 >> v2 >> v3;

            // Извлекаем индексы вершин (обрабатываем форматы v, v/vt, v/vt/vn)
            f.v1 = std::stoi(v1.substr(0, v1.find('/'))) - 1;
            f.v2 = std::stoi(v2.substr(0, v2.find('/'))) - 1;
            f.v3 = std::stoi(v3.substr(0, v3.find('/'))) - 1;
            outModel.faces.push_back(f);
        }
    }
    file.close();

    if (outModel.vertices.empty()) return false;

    // Автоматически вычисляем габариты (AABB) для корректной работы стрелок Gizmo и кликов
    outModel.xMin = outModel.xMax = outModel.vertices[0].x;
    outModel.yMin = outModel.yMax = outModel.vertices[0].y;
    outModel.zMin = outModel.zMax = outModel.vertices[0].z;

    for (const auto& v : outModel.vertices) {
        if (v.x < outModel.xMin) outModel.xMin = v.x; if (v.x > outModel.xMax) outModel.xMax = v.x;
        if (v.y < outModel.yMin) outModel.yMin = v.y; if (v.y > outModel.yMax) outModel.yMax = v.y;
        if (v.z < outModel.zMin) outModel.zMin = v.z; if (v.z > outModel.zMax) outModel.zMax = v.z;
    }
    return true;
}
