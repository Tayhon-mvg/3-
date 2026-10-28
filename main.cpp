#include "Gizmo.h" 
#include <windows.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#include <cmath>
#include <string>
#include <vector>
#include "SceneActions.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

// Глобальные переменные движка
HDC   dc = nullptr;
HGLRC rc = nullptr;
GLuint fontBase = 0;
HWND  MainWindow = nullptr;

int screenW = 800;
int screenH = 600;

float camX = 2.0f, camY = 1.0f, camZ = 3.0f;
float camYaw = -123.69f;
float camPitch = -15.5f;
float lookX = 0.0f, lookY = 0.0f, lookZ = -1.0f;
float fps = 0.0f;

std::vector<EditableCube> sceneCubes;
int selectedCubeIndex = -1;

// Переменные для перетаскивания Gizmo стрелок
GizmoAxis activeGizmoAxis = GIZMO_NONE;
float dragStartMouseX = 0, dragStartMouseY = 0;
float dragStartSizeX = 0, dragStartSizeY = 0, dragStartSizeZ = 0;

// Флаги триггеров кликов для главного цикла
bool triggerLeftClickPicking = false;
bool triggerDoubleClick = false;
bool triggerRightClickPicking = false;
int clickMouseX = 0;
int clickMouseY = 0;

EditableCube clipboardCube;
bool hasClipboardData = false;
float lastClickWorldX = 0.0f, lastClickWorldY = 0.0f, lastClickWorldZ = 0.0f;

// Прототипы функций
void SetPerspective(float fov, float aspect, float nearZ, float farZ);
void BuildFont(HWND hwnd);
void RenderText(float x, float y, const std::string& text);
void DrawEditableCube(const EditableCube& cube);
bool Init3D(HWND hwnd);
void GameUpdate(float dt);
void ScreenToWorldRay(int mouseX, int mouseY, float& outOriginX, float& outOriginY, float& outOriginZ, float& outDirX, float& outDirY, float& outDirZ);
int GetCubeUnderMouseRay(float ox, float oy, float oz, float dx, float dy, float dz);
GizmoAxis GetGizmoAxisUnderMouse(int mouseX, int mouseY, const EditableCube& cube);

void SetPerspective(float fov, float aspect, float nearZ, float farZ) {
    float fh = std::tan(fov * 3.14159265f / 360.0f) * nearZ;
    float fw = fh * aspect;
    glFrustum(-fw, fw, -fh, fh, nearZ, farZ);
}

void BuildFont(HWND hwnd) {
    HFONT font = CreateFontA(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Consolas");
    HDC hdc = GetDC(hwnd);
    SelectObject(hdc, font);
    fontBase = glGenLists(96);
    wglUseFontBitmapsA(hdc, 32, 96, fontBase);
    ReleaseDC(hwnd, hdc);
    DeleteObject(font);
}

void RenderText(float x, float y, const std::string& text) {
    glRasterPos2f(x, y);
    glListBase(fontBase - 32);
    glCallLists((GLsizei)text.length(), GL_UNSIGNED_BYTE, text.c_str());
}

// Отрисовка куба с красивым оранжевым Blender-контуром
void DrawEditableCube(const EditableCube& cube) {
    if (!cube.exists) return;

    glPushMatrix();
    glTranslatef(cube.posX, cube.posY, cube.posZ);

    if (cube.isCustomModel) {
        // --- ОТРИСОВКА ЗАГРУЖЕННОЙ 3D МОДЕЛИ ---
        glBegin(GL_TRIANGLES);
        if (cube.isLocked) glColor3f(0.5f, 0.1f, 0.1f);
        else glColor3f(0.6f, 0.6f, 0.6f); // Стандартный серый цвет сетки модели

        for (const auto& face : cube.faces) {
            // Рисуем треугольник по индексам из файла
            glVertex3f(cube.vertices[face.v1].x, cube.vertices[face.v1].y, cube.vertices[face.v1].z);
            glVertex3f(cube.vertices[face.v2].x, cube.vertices[face.v2].y, cube.vertices[face.v2].z);
            glVertex3f(cube.vertices[face.v3].x, cube.vertices[face.v3].y, cube.vertices[face.v3].z);
        }
        glEnd();
    }
    else {
        // --- СТАНДАРТНАЯ ОТРИСОВКА КУБА ---
        glBegin(GL_QUADS);
        for (int face = 0; face < 6; ++face) {
            if (cube.isLocked) glColor3f(0.5f, 0.1f, 0.1f);
            else {
                if (face == 0) glColor3f(0.6f, 0.6f, 0.6f); if (face == 1) glColor3f(0.4f, 0.4f, 0.4f);
                if (face == 2) glColor3f(0.7f, 0.7f, 0.7f); if (face == 3) glColor3f(0.3f, 0.3f, 0.3f);
                if (face == 4) glColor3f(0.5f, 0.5f, 0.5f); if (face == 5) glColor3f(0.5f, 0.5f, 0.5f);
            }
            if (face == 0) { glVertex3f(cube.xMin, cube.yMin, cube.zMax); glVertex3f(cube.xMax, cube.yMin, cube.zMax); glVertex3f(cube.xMax, cube.yMax, cube.zMax); glVertex3f(cube.xMin, cube.yMax, cube.zMax); }
            if (face == 1) { glVertex3f(cube.xMin, cube.yMin, cube.zMin); glVertex3f(cube.xMin, cube.yMax, cube.zMin); glVertex3f(cube.xMax, cube.yMax, cube.zMin); glVertex3f(cube.xMax, cube.yMin, cube.zMin); }
            if (face == 2) { glVertex3f(cube.xMin, cube.yMax, cube.zMin); glVertex3f(cube.xMin, cube.yMax, cube.zMax); glVertex3f(cube.xMax, cube.yMax, cube.zMax); glVertex3f(cube.xMax, cube.yMax, cube.zMin); }
            if (face == 3) { glVertex3f(cube.xMin, cube.yMin, cube.zMin); glVertex3f(cube.xMax, cube.yMin, cube.zMin); glVertex3f(cube.xMax, cube.yMin, cube.zMax); glVertex3f(cube.xMin, cube.yMin, cube.zMax); }
            if (face == 4) { glVertex3f(cube.xMax, cube.yMin, cube.zMin); glVertex3f(cube.xMax, cube.yMax, cube.zMin); glVertex3f(cube.xMax, cube.yMax, cube.zMax); glVertex3f(cube.xMax, cube.yMin, cube.zMax); }
            if (face == 5) { glVertex3f(cube.xMin, cube.yMin, cube.zMin); glVertex3f(cube.xMin, cube.yMin, cube.zMax); glVertex3f(cube.xMin, cube.yMax, cube.zMax); glVertex3f(cube.xMin, cube.yMax, cube.zMin); }
        }
        glEnd();
    }

    // Оранжевая обводка Blender-style автоматически подстроится под габариты модели!
    if (cube.isSelected && !cube.isLocked) {
        glDisable(GL_DEPTH_TEST); glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glLineWidth(3.0f); glColor3f(1.0f, 0.5f, 0.0f);
        glBegin(GL_QUADS);
        for (int face = 0; face < 6; ++face) {
            if (face == 0) { glVertex3f(cube.xMin, cube.yMin, cube.zMax); glVertex3f(cube.xMax, cube.yMin, cube.zMax); glVertex3f(cube.xMax, cube.yMax, cube.zMax); glVertex3f(cube.xMin, cube.yMax, cube.zMax); }
            if (face == 1) { glVertex3f(cube.xMin, cube.yMin, cube.zMin); glVertex3f(cube.xMin, cube.yMax, cube.zMin); glVertex3f(cube.xMax, cube.yMax, cube.zMin); glVertex3f(cube.xMax, cube.yMin, cube.zMin); }
            if (face == 2) { glVertex3f(cube.xMin, cube.yMax, cube.zMin); glVertex3f(cube.xMin, cube.yMax, cube.zMax); glVertex3f(cube.xMax, cube.yMax, cube.zMax); glVertex3f(cube.xMax, cube.yMax, cube.zMin); }
            if (face == 3) { glVertex3f(cube.xMin, cube.yMin, cube.zMin); glVertex3f(cube.xMax, cube.yMin, cube.zMin); glVertex3f(cube.xMax, cube.yMin, cube.zMax); glVertex3f(cube.xMin, cube.yMin, cube.zMax); }
            if (face == 4) { glVertex3f(cube.xMax, cube.yMin, cube.zMin); glVertex3f(cube.xMax, cube.yMax, cube.zMin); glVertex3f(cube.xMax, cube.yMax, cube.zMax); glVertex3f(cube.xMax, cube.yMin, cube.zMax); }
            if (face == 5) { glVertex3f(cube.xMin, cube.yMin, cube.zMin); glVertex3f(cube.xMin, cube.yMin, cube.zMax); glVertex3f(cube.xMin, cube.yMax, cube.zMax); glVertex3f(cube.xMin, cube.yMax, cube.zMin); }
        }
        glEnd();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); glLineWidth(1.0f); glEnable(GL_DEPTH_TEST);
    }
    glPopMatrix();
}


// Поиск ближайшей стрелки методом 2D-проекции кончиков осей на экран (100% попадание на ЛКМ)
GizmoAxis GetGizmoAxisUnderMouse(int mouseX, int mouseY, const EditableCube& cube) {
    GLint viewport[4];       // ИСПРАВЛЕНО: Теперь это честный массив из 4 элементов
    GLdouble modelview[16];   // ИСПРАВЛЕНО: Матрица 4x4 (16 элементов)
    GLdouble projection[16];  // ИСПРАВЛЕНО: Матрица 4x4 (16 элементов)

    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);

    // ИСПРАВЛЕНО: Двумерный массив [3][3] под 3D-координаты кончиков стрелок
    double arrowPos[3][3] = {
        { (double)cube.posX + 1.5, (double)cube.posY, (double)cube.posZ }, // X
        { (double)cube.posX, (double)cube.posY + 1.5, (double)cube.posZ }, // Y
        { (double)cube.posX, (double)cube.posY, (double)cube.posZ + 1.5 }  // Z
    };

    double winX, winY, winZ;
    float minClickDistance = 45.0f;
    GizmoAxis detectedAxis = GIZMO_NONE;

    for (int i = 0; i < 3; ++i) {
        // ИСПРАВЛЕНО: Теперь передаем конкретные ячейки массива [i][0], [i][1], [i][2]
        if (gluProject(arrowPos[i][0], arrowPos[i][1], arrowPos[i][2], modelview, projection, viewport, &winX, &winY, &winZ)) {
            float screenArrowY = (float)viewport[3] - (float)winY; // ИСПРАВЛЕНО: Высота экрана берется из viewport[3]
            float dx = (float)mouseX - (float)winX;
            float dy = (float)mouseY - screenArrowY;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < minClickDistance) {
                minClickDistance = dist;
                detectedAxis = (GizmoAxis)(i + 1);
            }
        }
    }
    return detectedAxis;
}


// Генерация 3D-луча взгляда из курсора мыши для двойного клика
// --- ЗАМЕНИТЕ ЭТУ ФУНКЦИЮ ПОЛНОСТЬЮ ---
void ScreenToWorldRay(int mouseX, int mouseY, float& outOriginX, float& outOriginY, float& outOriginZ, float& outDirX, float& outDirY, float& outDirZ) {
    // ЗАМЕНИ ЭТИ СТРОКИ В НАЧАЛЕ ScreenToWorldRay:
    GLint viewport[4];       // Теперь это честный массив из 4 элементов
    GLdouble modelview[16];  // Матрица 4x4 (16 элементов)
    GLdouble projection[16]; // Матрица 4x4 (16 элементов)
    GLdouble posX, posY, posZ;

    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);

    // В OpenGL координата Y растет снизу вверх, переворачиваем её относительно высоты окна
    GLdouble openGLMouseY = (GLdouble)(viewport[3] - mouseY);

    // Считаем точку на ближней плоскости отсечения (Z = 0.0)
    gluUnProject((GLdouble)mouseX, openGLMouseY, 0.0, modelview, projection, viewport, &posX, &posY, &posZ);
    outOriginX = (float)posX; outOriginY = (float)posY; outOriginZ = (float)posZ;

    // Считаем точку на дальней плоскости отсечения (Z = 1.0)
    GLdouble posXFar, posYFar, posZFar;
    gluUnProject((GLdouble)mouseX, openGLMouseY, 1.0, modelview, projection, viewport, &posXFar, &posYFar, &posZFar);

    // Вычисляем вектор направления луча мыши
    float dx = (float)posXFar - (float)posX;
    float dy = (float)posYFar - (float)posY;
    float dz = (float)posZFar - (float)posZ;
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);

    // Нормализуем вектор (приводим к длине = 1.0)
    if (len > 0.001f) {
        outDirX = dx / len;
        outDirY = dy / len;
        outDirZ = dz / len;
    }
    else {
        outDirX = 0.0f;
        outDirY = 0.0f;
        outDirZ = -1.0f;
    }
}


// Математический ААВВ-тест пересечения луча с кубом (Идеально для двойного клика) [cite: 1]
int GetCubeUnderMouseRay(float ox, float oy, float oz, float dx, float dy, float dz) {
    int closestIndex = -1; float minT = 99999.0f;
    for (size_t i = 0; i < sceneCubes.size(); ++i) {
        if (!sceneCubes[i].exists) continue;
        const auto& cube = sceneCubes[i];
        float lOx = ox - cube.posX; float lOy = oy - cube.posY; float lOz = oz - cube.posZ;

        float tmin = (cube.xMin - lOx) / (dx + 0.00001f); float tmax = (cube.xMax - lOx) / (dx + 0.00001f);
        if (tmin > tmax) std::swap(tmin, tmax);
        float tymin = (cube.yMin - lOy) / (dy + 0.00001f); float tymax = (cube.yMax - lOy) / (dy + 0.00001f);
        if (tymin > tymax) std::swap(tymin, tymax);
        if ((tmin > tymax) || (tymin > tmax)) continue;
        if (tymin > tmin) tmin = tymin;
        if (tymax < tmax) tmax = tymax;
        float tzmin = (cube.zMin - lOz) / (dz + 0.00001f); float tzmax = (cube.zMax - lOz) / (dz + 0.00001f);
        if (tzmin > tzmax) std::swap(tzmin, tzmax);
        if((tmin > tzmax) || (tzmin > tmax)) continue;
        if (tzmin > tmin) tmin = tzmin;
        if (tzmax < tmax) tmax = tzmax;
        if (tmin > 0.0f && tmin < minT) { minT = tmin; closestIndex = (int)i; }
    }
    return closestIndex;
}
bool Init3D(HWND hwnd) {
    PIXELFORMATDESCRIPTOR pfd = {}; pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 24; pfd.cDepthBits = 32;
    dc = GetDC(hwnd); int fmt = ChoosePixelFormat(dc, &pfd);
    if (fmt == 0 || !SetPixelFormat(dc, fmt, &pfd)) return false;
    rc = wglCreateContext(dc); if (rc == nullptr) return false;
    wglMakeCurrent(dc, rc); glEnable(GL_DEPTH_TEST); BuildFont(hwnd);
    EditableCube mainCube; sceneCubes.push_back(mainCube);
    return true;
}
void GameUpdate(float dt) {
    if (camPitch > 89.0f)  camPitch = 89.0f;
    if (camPitch < -89.0f) camPitch = -89.0f;

    float yawRad = camYaw * 3.14159265f / 180.0f;
    float pitchRad = camPitch * 3.14159265f / 180.0f;

    lookX = std::cos(pitchRad) * std::cos(yawRad);
    lookY = std::sin(pitchRad);
    lookZ = std::cos(pitchRad) * std::sin(yawRad);

    float speed = 4.0f * dt; // Скорость полёта камеры

    if (activeGizmoAxis == GIZMO_NONE) {
        // Стандартное перемещение по горизонтали/взгляду (WASD)
        if (GetAsyncKeyState('W') & 0x8000) { camX += lookX * speed; camY += lookY * speed; camZ += lookZ * speed; }
        if (GetAsyncKeyState('S') & 0x8000) { camX -= lookX * speed; camY -= lookY * speed; camZ -= lookZ * speed; }
        if (GetAsyncKeyState('A') & 0x8000) { camX -= -lookZ * speed; camZ -= lookX * speed; }
        if (GetAsyncKeyState('D') & 0x8000) { camX += -lookZ * speed; camZ -= lookX * speed; }

        // --- ДОБАВЛЕНО: Вертикальный полёт (Пробел и Левый Shift) ---
        // VK_SPACE — Пробел (взлёт строго вверх по оси Y)
        if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
            camY += speed;
        }
        // VK_LSHIFT — Левый Shift (спуск строго вниз по оси Y)
        if (GetAsyncKeyState(VK_LSHIFT) & 0x8000) {
            camY -= speed;
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static int lastX = 0, lastY = 0;
    switch (msg) {
    case WM_SIZE:
        screenW = LOWORD(lp); screenH = HIWORD(lp); if (screenH == 0) screenH = 1;
        glViewport(0, 0, screenW, screenH); break;
    case WM_LBUTTONDOWN: {
        clickMouseX = LOWORD(lp); clickMouseY = HIWORD(lp);
        triggerLeftClickPicking = true;
        dragStartMouseX = (float)clickMouseX; dragStartMouseY = (float)clickMouseY;
        lastX = clickMouseX; lastY = clickMouseY; break;
    }
    case WM_LBUTTONDBLCLK: {
        clickMouseX = LOWORD(lp); clickMouseY = HIWORD(lp);
        triggerDoubleClick = true; break;
    }
    case WM_MOUSEMOVE: {
        int mouseX = LOWORD(lp); int mouseY = HIWORD(lp);
        if (wp & MK_LBUTTON && activeGizmoAxis != GIZMO_NONE && selectedCubeIndex != -1) {
            float deltaX = (float)(mouseX - dragStartMouseX) * 0.015f;
            float deltaY = (float)(mouseY - dragStartMouseY) * -0.015f;
            auto& cube = sceneCubes[selectedCubeIndex];
            if (activeGizmoAxis == GIZMO_X) { float nX = dragStartSizeX + deltaX * 2.0f; if (nX < 0.1f) nX = 0.1f; cube.xMin = -nX / 2.0f; cube.xMax = nX / 2.0f; }
            if (activeGizmoAxis == GIZMO_Y) { float nY = dragStartSizeY + deltaY * 2.0f; if (nY < 0.1f) nY = 0.1f; cube.yMin = -nY / 2.0f; cube.yMax = nY / 2.0f; }
            if (activeGizmoAxis == GIZMO_Z) { float nZ = dragStartSizeZ + (deltaX - deltaY) * 1.0f; if (nZ < 0.1f) nZ = 0.1f; cube.zMin = -nZ / 2.0f; cube.zMax = nZ / 2.0f; }
        }
        else if (wp & MK_LBUTTON && activeGizmoAxis == GIZMO_NONE) {
            camYaw += (mouseX - lastX) * 0.2f; camPitch -= (mouseY - lastY) * 0.2f;
        }
        lastX = mouseX; lastY = mouseY; break;
    }
    case WM_LBUTTONUP: activeGizmoAxis = GIZMO_NONE; break;
    case WM_RBUTTONDOWN: {
        clickMouseX = LOWORD(lp); clickMouseY = HIWORD(lp);
        triggerRightClickPicking = true;
        lastClickWorldX = camX + lookX * 3.0f; lastClickWorldY = camY + lookY * 3.0f; lastClickWorldZ = camZ + lookZ * 3.0f;
        POINT p = { clickMouseX, clickMouseY }; ClientToScreen(hwnd, &p);
        HMENU hMenu = CreatePopupMenu();
        if (selectedCubeIndex != -1) {
            bool isLocked = sceneCubes[selectedCubeIndex].isLocked;
            AppendMenuW(hMenu, isLocked ? MF_GRAYED : MF_STRING, ID_MENU_DELETE, L"Удалить (Delete)");
            AppendMenuW(hMenu, isLocked ? MF_GRAYED : MF_STRING, ID_MENU_COPY, L"Копировать (Copy)");
            AppendMenuW(hMenu, isLocked ? MF_GRAYED : MF_STRING, ID_MENU_CUT, L"Вырезать (Cut)");
            AppendMenuW(hMenu, MF_STRING, ID_MENU_LOCK, isLocked ? L"Открепить (Unlock)" : L"Закрепить (Lock)");
        }
        else { AppendMenuW(hMenu, hasClipboardData ? MF_STRING : MF_GRAYED, ID_MENU_PASTE, L"Вставить (Paste)"); }
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, p.x, p.y, 0, hwnd, nullptr); DestroyMenu(hMenu); break;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wp);
        if (wmId == ID_MENU_IMPORT_OBJ) {
            const wchar_t* filePath = Open3DFileDialog(hwnd);
            if (filePath != nullptr && filePath[0] != L'\0') {
                EditableCube newModel;
                // Загружаем геометрию из выбранного .obj файла
                if (LoadOBJModel(filePath, newModel)) {
                    // Спавним объект строго перед глазами пользователя в 3D пространстве
                    newModel.posX = camX + lookX * 3.0f;
                    newModel.posY = camY + lookY * 3.0f;
                    newModel.posZ = camZ + lookZ * 3.0f;

                    newModel.isSelected = false;
                    newModel.isLocked = false;
                    newModel.exists = true;

                    // Добавляем модель в общий список сцены
                    sceneCubes.push_back(newModel);
                }
                else {
                    MessageBoxW(hwnd, L"Не удалось прочитать или распарсить OBJ файл.", L"Ошибка импорта", MB_OK | MB_ICONERROR);
                }
            }
        }
        else {
            ExecuteSceneAction(wmId, sceneCubes, selectedCubeIndex, clipboardCube, hasClipboardData, lastClickWorldX, lastClickWorldY, lastClickWorldZ);
        }
        break;
    }


    case WM_DESTROY: glDeleteLists(fontBase, 96); PostQuitMessage(0); break;
    default: return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show) {
    const wchar_t myClassName[] = L"EngineWindow"; WNDCLASSW w = {}; w.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
    w.lpfnWndProc = WndProc; w.hInstance = hInst; w.hCursor = LoadCursor(nullptr, IDC_ARROW); w.lpszClassName = myClassName;
    RegisterClassW(&w);
        // 1. Создаем меню
    HMENU hTopMenu = CreateMenu();
    HMENU hFileSubMenu = CreatePopupMenu();
    AppendMenuW(hFileSubMenu, MF_STRING, ID_MENU_IMPORT_OBJ, L"Импортировать 3D файл (.obj)...");
    AppendMenuW(hTopMenu, MF_POPUP, (UINT_PTR)hFileSubMenu, L"Файл");
    
    // 2. Объявляем переменнную окна ИМЕННО ЗДЕСЬ (добавлено HWND):
    HWND hwnd = CreateWindowExW(0, myClassName, L"Blender Gizmo Editor Hybrid",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 800, 600,
        nullptr, hTopMenu, hInst, nullptr);

    // 3. Передаем в глобальные переменные движка и запускаем
    MainWindow = hwnd; 
    ShowWindow(hwnd, show); 
    if (!Init3D(hwnd)) return 0;


    LARGE_INTEGER freq, t1, t2; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&t1);
    float acc = 0.0f; int frames = 0; MSG msg = {}; std::string uiText = "";
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        else {
            QueryPerformanceCounter(&t2); float dt = (float)(t2.QuadPart - t1.QuadPart) / freq.QuadPart; t1 = t2;
            if (dt > 0.1f) dt = 0.1f; acc += dt; frames++;
            if (acc >= 0.5f) { fps = frames / acc; frames = 0; acc = 0.0f; char buf[64]; snprintf(buf, sizeof(buf), "FPS: %d | Cubes: %zu", (int)fps, sceneCubes.size()); uiText = buf; }
            GameUpdate(dt);
            glClearColor(0.2f, 0.2f, 0.2f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glMatrixMode(GL_PROJECTION); glLoadIdentity(); SetPerspective(60.0f, (float)screenW / screenH, 0.1f, 100.0f);
            glMatrixMode(GL_MODELVIEW); glLoadIdentity(); gluLookAt(camX, camY, camZ, camX + lookX, camY + lookY, camZ + lookZ, 0.0f, 1.0f, 0.0f);
            // --- ГИБРИДНЫЙ ОБСЧЕТ КЛИКОВ МЫШИ (СТРОГО ПОД МАТРИЦАМИ) ---
            // 1. Одиночный ЛКМ: Ищет 2D-пиксели стрелочек на экране (Метод Проекций)
            if (triggerLeftClickPicking) {
                if (selectedCubeIndex != -1 && !sceneCubes[selectedCubeIndex].isLocked) {
                    activeGizmoAxis = GetGizmoAxisUnderMouse(clickMouseX, clickMouseY, sceneCubes[selectedCubeIndex]);
                    if (activeGizmoAxis != GIZMO_NONE) {
                        const auto& cube = sceneCubes[selectedCubeIndex];
                        dragStartSizeX = cube.xMax - cube.xMin;
                        dragStartSizeY = cube.yMax - cube.yMin;
                        dragStartSizeZ = cube.zMax - cube.zMin;
                    }
                }
                triggerLeftClickPicking = false;
            }
            // 2. Двойной ЛКМ: Ищет 3D-геометрию куба в пространстве (Метод Лучей)
            if (triggerDoubleClick) {
                float ox, oy, oz, dx, dy, dz;
                ScreenToWorldRay(clickMouseX, clickMouseY, ox, oy, oz, dx, dy, dz);
                int clickedCube = GetCubeUnderMouseRay(ox, oy, oz, dx, dy, dz);
                if (clickedCube != -1) {
                    if (selectedCubeIndex == clickedCube) {
                        sceneCubes[selectedCubeIndex].isSelected = false;
                        selectedCubeIndex = -1;
                    }
                    else {
                        if (selectedCubeIndex != -1) sceneCubes[selectedCubeIndex].isSelected = false;
                        selectedCubeIndex = clickedCube;
                        sceneCubes[selectedCubeIndex].isSelected = true;
                    }
                }
                else {
                    if (selectedCubeIndex != -1) {
                        sceneCubes[selectedCubeIndex].isSelected = false;
                        selectedCubeIndex = -1;
                    }
                }
                triggerDoubleClick = false;
            }
            if (triggerRightClickPicking) {
                triggerRightClickPicking = false; // ПКМ только для вызова меню
            }
            // Рендеринг сцены
            for (const auto& cube : sceneCubes) DrawEditableCube(cube);
            if (selectedCubeIndex != -1 && !sceneCubes[selectedCubeIndex].isLocked) DrawGizmoArrows(sceneCubes[selectedCubeIndex]);
            // Текст интерфейса
            glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, screenW, 0, screenH, -1, 1);
            glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glDisable(GL_DEPTH_TEST); glColor3f(1.0f, 1.0f, 1.0f);
            RenderText(15.0f, (float)screenH - 25.0f, uiText);
            RenderText(15.0f, 25.0f, "Double-LMB on Cube: Select (Orange Outline) | LMB Drag on Arrows: Resize Cube | RMB: Menu");
            if (selectedCubeIndex != -1) { std::string s = "Editing Cube #" + std::to_string(selectedCubeIndex); if (sceneCubes[selectedCubeIndex].isLocked) s += " [LOCKED]"; RenderText(15.0f, (float)screenH - 45.0f, s); }
            glEnable(GL_DEPTH_TEST); SwapBuffers(dc);
        }
    }
    wglMakeCurrent(nullptr, nullptr); wglDeleteContext(rc); ReleaseDC(hwnd, dc); return 0;
}
    