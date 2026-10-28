#include "SceneActions.h"
#include <commdlg.h>
#include <fstream>
#include <sstream>
// ���������� ����������� ����� ��� �������� ���� � �����
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

// ����� ������� ���������� Windows ��� ������������� std::wstring
const wchar_t* Open3DFileDialog(HWND hwnd) {
    g_SelectedFilePath[0] = L'\0';

    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = g_SelectedFilePath;
    ofn.nMaxFile = MAX_PATH;

    // ИСПРАВЛЕНО: Теперь проводник видит абсолютно ВСЕ форматы 3D-моделей
    ofn.lpstrFilter = L"Все поддерживаемые 3D файлы (*.obj; *.3mf; *.fbx; *.dae; *.stl; *.gltf; *.glb; *.ply; *.3ds; *.blend)\0*.obj;*.3mf;*.fbx;*.dae;*.stl;*.gltf;*.glb;*.ply;*.3ds;*.blend\0"
        L"Wavefront OBJ (*.obj)\0*.obj\0"
        L"3D Manufacturing (*.3mf)\0*.3mf\0"
        L"Autodesk FBX (*.fbx)\0*.fbx\0"
        L"Collada DAE (*.dae)\0*.dae\0"
        L"Stereolithography (*.stl)\0*.stl\0"
        L"glTF Format (*.gltf; *.glb)\0*.gltf;*.glb\0"
        L"Все файлы (*.*)\0*.*\0";

    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        return g_SelectedFilePath;
    }
    return nullptr;
}

bool LoadOBJModel(const wchar_t* filename, EditableCube& outModel) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;

    outModel.vertices.clear();
    outModel.faces.clear();
    outModel.isCustomModel = true;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();

        std::string lowerLine = line;
        for (auto& c : lowerLine) c = std::tolower(c);

        // --- УНИВЕРСАЛЬНЫЙ XML-ПАРСЕР ДЛЯ ЛЮБЫХ 3MF ФАЙЛОВ ---
        if (lowerLine.find("<vertex") != std::string::npos) {
            Vertex3D v;
            size_t xPos = lowerLine.find("x=\"");
            size_t yPos = lowerLine.find("y=\"");
            size_t zPos = lowerLine.find("z=\"");
            if (xPos != std::string::npos && yPos != std::string::npos && zPos != std::string::npos) {
                v.x = std::stof(line.substr(xPos + 3));
                v.y = std::stof(line.substr(yPos + 3));
                v.z = std::stof(line.substr(zPos + 3));
                outModel.vertices.push_back(v);
            }
            continue;
        }
        if (lowerLine.find("<triangle") != std::string::npos) {
            Face3D f;
            size_t v1Pos = lowerLine.find("v1=\"");
            size_t v2Pos = lowerLine.find("v2=\"");
            size_t v3Pos = lowerLine.find("v3=\"");
            if (v1Pos != std::string::npos && v2Pos != std::string::npos && v3Pos != std::string::npos) {
                f.v1 = std::stoi(line.substr(v1Pos + 4));
                f.v2 = std::stoi(line.substr(v2Pos + 4));
                f.v3 = std::stoi(line.substr(v3Pos + 4));
                outModel.faces.push_back(f);
            }
            continue;
        }

        // --- БРОНЕБОЙНЫЙ ПАРСЕР ДЛЯ ЛЮБЫХ МОДИФИКАЦИЙ OBJ ---
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "v") {
            Vertex3D v;
            ss >> v.x >> v.y >> v.z;
            outModel.vertices.push_back(v);
        }
        else if (type == "f") {
            std::string token;
            std::vector<int> faceIndices;

            // Читаем все индексы в строке (поддерживает и треугольники, и квады, и полигоны)
            while (ss >> token) {
                // Если индекс записан как v/vt/vn или v//vn — отсекаем всё, что после первого слэша
                size_t slashPos = token.find('/');
                std::string indexStr = (slashPos == std::string::npos) ? token : token.substr(0, slashPos);

                if (!indexStr.empty()) {
                    int idx = std::stoi(indexStr);
                    // Обрабатываем относительные отрицательные индексы, если они есть в OBJ
                    idx = (idx < 0) ? (int)outModel.vertices.size() + idx : idx - 1;
                    faceIndices.push_back(idx);
                }
            }

            // Автоматическая триангуляция (если модель состоит из квадов, бьем их на треугольники на лету)
            if (faceIndices.size() >= 3) {
                for (size_t k = 1; k < faceIndices.size() - 1; ++k) {
                    Face3D f;
                    f.v1 = faceIndices[0];
                    f.v2 = faceIndices[k];
                    f.v3 = faceIndices[k + 1];
                    outModel.faces.push_back(f);
                }
            }
        }
    }
    file.close();

    if (outModel.vertices.empty() || outModel.faces.empty()) return false;

    // Авто-центрирование геометрии в локальный ноль (0,0,0) объекта
    float minX = outModel.vertices[0].x, maxX = outModel.vertices[0].x;
    float minY = outModel.vertices[0].y, maxY = outModel.vertices[0].y;
    float minZ = outModel.vertices[0].z, maxZ = outModel.vertices[0].z;

    for (const auto& v : outModel.vertices) {
        if (v.x < minX) minX = v.x; if (v.x > maxX) maxX = v.x;
        if (v.y < minY) minY = v.y; if (v.y > maxY) maxY = v.y;
        if (v.z < minZ) minZ = v.z; if (v.z > maxZ) maxZ = v.z;
    }

    float centerX = (minX + maxX) * 0.5f;
    float centerY = (minY + maxY) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;

    for (auto& v : outModel.vertices) {
        v.x -= centerX; v.y -= centerY; v.z -= centerZ;
    }

    // Умный авто-масштаб под экран для абсолютно любых размеров сеток
    float sizeX = maxX - minX; float sizeY = maxY - minY; float sizeZ = maxZ - minZ;
    float maxDimension = sizeX;
    if (sizeY > maxDimension) maxDimension = sizeY;
    if (sizeZ > maxDimension) maxDimension = sizeZ;

    if (maxDimension > 3.0f) {
        float scaleFactor = 1.5f / maxDimension;
        for (auto& v : outModel.vertices) {
            v.x *= scaleFactor; v.y *= scaleFactor; v.z *= scaleFactor;
        }
        minX *= scaleFactor; maxX *= scaleFactor;
        minY *= scaleFactor; maxY *= scaleFactor;
        minZ *= scaleFactor; maxZ *= scaleFactor;
        centerX *= scaleFactor; centerY *= scaleFactor; centerZ *= scaleFactor;
    }

    outModel.xMin = minX - centerX; outModel.xMax = maxX - centerX;
    outModel.yMin = minY - centerY; outModel.yMax = maxY - centerY;
    outModel.zMin = minZ - centerZ; outModel.zMax = maxZ - centerZ;

    return true;
}
