#pragma once
#include <windows.h>
#include <gl/GL.h>
#include <vector>

enum GizmoAxis { GIZMO_NONE = 0, GIZMO_X = 1, GIZMO_Y = 2, GIZMO_Z = 3 };

// Структура для хранения одной 3D-вершины файла
struct Vertex3D {
    float x, y, z;
};

// Структура для хранения одного полигона (треугольника) файла
struct Face3D {
    int v1, v2, v3;
};

struct EditableCube {
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;

    // Размеры для куба
    float xMin = -0.5f, xMax = 0.5f;
    float yMin = -0.5f, yMax = 0.5f;
    float zMin = -0.5f, zMax = 0.5f;

    // Данные для загруженного файла OBJ
    bool isCustomModel = false;
    std::vector<Vertex3D> vertices;
    std::vector<Face3D> faces;

    bool isSelected = false;
    bool isLocked = false;
    bool exists = true;
};

void DrawGizmoArrows(const EditableCube& cube);
