#pragma once
#include <windows.h>
#include <gl/GL.h>
#include <vector>

enum GizmoAxis { GIZMO_NONE = 0, GIZMO_X = 1, GIZMO_Y = 2, GIZMO_Z = 3 };

struct Vertex3D {
    float x, y, z;
};

struct Face3D {
    int v1, v2, v3;
};

struct EditableCube {
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    unsigned int displayListId = 0; // ИСПРАВЛЕНО: Перенесли в самый верх структуры

    // Границы для куба
    float xMin = -0.5f, xMax = 0.5f;
    float yMin = -0.5f, yMax = 0.5f;
    float zMin = -0.5f, zMax = 0.5f;

    // Переменные аппаратного масштабирования для видеокарты (для высокого FPS)
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float scaleZ = 1.0f;

    bool isCustomModel = false;
    std::vector<Vertex3D> vertices;
    std::vector<Face3D> faces;

    bool isSelected = false;
    bool isLocked = false;
    bool exists = true;
};


void DrawGizmoArrows(const EditableCube& cube);
