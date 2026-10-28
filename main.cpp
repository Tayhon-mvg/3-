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

GizmoAxis activeGizmoAxis = GIZMO_NONE;
float dragStartMouseX = 0, dragStartMouseY = 0;
float dragStartSizeX = 0, dragStartSizeY = 0, dragStartSizeZ = 0;

bool triggerLeftClickPicking = false;
bool triggerDoubleClick = false;
bool triggerRightClickPicking = false;
int clickMouseX = 0;
int clickMouseY = 0;

EditableCube clipboardCube;
bool hasClipboardData = false;
float lastClickWorldX = 0.0f, lastClickWorldY = 0.0f, lastClickWorldZ = 0.0f;

void SetPerspective(float fov, float aspect, float nearZ, float farZ);
void BuildFont(HWND hwnd);
void RenderText(float x, float y, const std::string& text);
void DrawEditableCube(const EditableCube& cube);
bool Init3D(HWND hwnd);
void GameUpdate(float dt);
void ScreenToWorldRay(int mouseX, int mouseY, float& outOriginX, float& outOriginY, float& outOriginZ, float& outDirX, float& outDirY, float& outDirZ);
int GetCubeUnderMouseRay(float ox, float oy, float oz, float dx, float dy, float dz);
GizmoAxis GetGizmoAxisUnderMouse(int mouseX, int mouseY, const EditableCube& cube);
// === ПЕРЕНЕСЕННЫЙ КОД ОТРИСОВКИ СТРЕЛОЧЕК ГИЗМО ===
void DrawGizmoArrows(const EditableCube& cube) {
    glDisable(GL_DEPTH_TEST);
    glPushMatrix();
    glTranslatef(cube.posX, cube.posY, cube.posZ);

    GLUquadric* quad = gluNewQuadric();
    gluQuadricDrawStyle(quad, GLU_FILL);

    float stickRadius = 0.02f;
    float stickLength = 1.2f;
    float coneRadius = 0.07f;
    float coneLength = 0.3f;
    int segments = 12;

    // Ось X - Красная
    glPushMatrix();
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
    glColor3f(0.9f, 0.1f, 0.1f);
    gluCylinder(quad, stickRadius, stickRadius, stickLength, segments, 1);
    glTranslatef(0.0f, 0.0f, stickLength);
    gluCylinder(quad, coneRadius, 0.0f, coneLength, segments, 1);
    glPopMatrix();

    // Ось Y - Зеленая
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glColor3f(0.1f, 0.9f, 0.1f);
    gluCylinder(quad, stickRadius, stickRadius, stickLength, segments, 1);
    glTranslatef(0.0f, 0.0f, stickLength);
    gluCylinder(quad, coneRadius, 0.0f, coneLength, segments, 1);
    glPopMatrix();

    // Ось Z - Синяя
    glPushMatrix();
    glColor3f(0.1f, 0.1f, 0.9f);
    gluCylinder(quad, stickRadius, stickRadius, stickLength, segments, 1);
    glTranslatef(0.0f, 0.0f, stickLength);
    gluCylinder(quad, coneRadius, 0.0f, coneLength, segments, 1);
    glPopMatrix();

    gluDeleteQuadric(quad);
    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
}

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

void DrawEditableCube(const EditableCube& cube) {
    if (!cube.exists) return;

    glPushMatrix(); // Матрицу создаем ровно ОДИН раз
    glTranslatef(cube.posX, cube.posY, cube.posZ);

    if (cube.isCustomModel) {
        // Аппаратное масштабирование ТОЛЬКО для кастомных 3D-моделей
        glScalef(cube.scaleX, cube.scaleY, cube.scaleZ);

        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_COLOR_MATERIAL);
        glEnable(GL_AUTO_NORMAL);

        float lightPos[] = { 3.0f, 6.0f, 3.0f, 1.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

        if (cube.isLocked) glColor3f(0.5f, 0.1f, 0.1f);
        else glColor3f(0.6f, 0.6f, 0.6f);

        EditableCube& mutableCube = const_cast<EditableCube&>(cube);

        // Компиляция модели в VRAM видеокарты (происходит один раз при импорте)
        if (mutableCube.displayListId == 0) {
            mutableCube.displayListId = glGenLists(1);
            glNewList(mutableCube.displayListId, GL_COMPILE);
            glBegin(GL_TRIANGLES);
            for (const auto& face : cube.faces) {
                glVertex3f(cube.vertices[face.v1].x, cube.vertices[face.v1].y, cube.vertices[face.v1].z);
                glVertex3f(cube.vertices[face.v2].x, cube.vertices[face.v2].y, cube.vertices[face.v2].z);
                glVertex3f(cube.vertices[face.v3].x, cube.vertices[face.v3].y, cube.vertices[face.v3].z);
            }
            glEnd();
            glEndList();
        }

        glCallList(mutableCube.displayListId); // Сверхбыстрый рендеринг без циклов процессора
        glDisable(GL_LIGHTING);
    }
    else {
        // --- ТВОЙ СТАНДАРТНЫЙ РАБОЧИЙ КОД ОТРИСОВКИ КУБА ---
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

    // Твой старый код оранжевого контура выделения (Blender-style Outline) для выделенного объекта
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

    glPopMatrix(); // Закрываем матрицу ровно ОДИН раз в самом конце функции
}


GizmoAxis GetGizmoAxisUnderMouse(int mouseX, int mouseY, const EditableCube& cube) {
    GLint viewport[4];       // ФИКС ТИПА МАССИВА
    GLdouble modelview[16];   // ФИКС ТИПА МАССИВА
    GLdouble projection[16];  // ФИКС ТИПА МАССИВА

    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);

    double arrowPos[3][3] = {
        { (double)cube.posX + 1.2, (double)cube.posY, (double)cube.posZ },
        { (double)cube.posX, (double)cube.posY + 1.2, (double)cube.posZ },
        { (double)cube.posX, (double)cube.posY, (double)cube.posZ + 1.2 }
    };

    double winX, winY, winZ;
    float minClickDistance = 45.0f;
    GizmoAxis detectedAxis = GIZMO_NONE;

    for (int i = 0; i < 3; ++i) {
        if (gluProject(arrowPos[i][0], arrowPos[i][1], arrowPos[i][2], modelview, projection, viewport, &winX, &winY, &winZ)) {
            float screenArrowY = (float)viewport[3] - (float)winY;
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

void ScreenToWorldRay(int mouseX, int mouseY, float& outOriginX, float& outOriginY, float& outOriginZ, float& outDirX, float& outDirY, float& outDirZ) {
    GLint viewport[4];       // ФИКС ТИПА МАССИВА
    GLdouble modelview[16];  // ФИКС ТИПА МАССИВА
    GLdouble projection[16]; // ФИКС ТИПА МАССИВА
    GLdouble posX, posY, posZ;

    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);

    GLdouble openGLMouseY = (GLdouble)(viewport[3] - mouseY);

    gluUnProject((GLdouble)mouseX, openGLMouseY, 0.0, modelview, projection, viewport, &posX, &posY, &posZ);
    outOriginX = (float)posX; outOriginY = (float)posY; outOriginZ = (float)posZ;

    GLdouble posXFar, posYFar, posZFar;
    gluUnProject((GLdouble)mouseX, openGLMouseY, 1.0, modelview, projection, viewport, &posXFar, &posYFar, &posZFar);

    float dx = (float)posXFar - (float)posX;
    float dy = (float)posYFar - (float)posY;
    float dz = (float)posZFar - (float)posZ;
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (len > 0.001f) {
        outDirX = dx / len; outDirY = dy / len; outDirZ = dz / len;
    }
    else {
        outDirX = 0.0f; outDirY = 0.0f; outDirZ = -1.0f;
    }
}

int GetCubeUnderMouseRay(float ox, float oy, float oz, float dx, float dy, float dz) {
    int closestIndex = -1; float minT = 99999.0f;
    for (size_t i = 0; i < sceneCubes.size(); ++i) {
        if (!sceneCubes[i].exists) continue;
        const auto& cube = sceneCubes[i];
        float lOx = ox - cube.posX; float lOy = oy - cube.posY; float lOz = oz - cube.posZ;

        float t1 = (cube.xMin - lOx) / (dx == 0.0f ? 0.00001f : dx);
        float t2 = (cube.xMax - lOx) / (dx == 0.0f ? 0.00001f : dx);
        float t3 = (cube.yMin - lOy) / (dy == 0.0f ? 0.00001f : dy);
        float t4 = (cube.yMax - lOy) / (dy == 0.0f ? 0.00001f : dy);
        float t5 = (cube.zMin - lOz) / (dz == 0.0f ? 0.00001f : dz);
        float t6 = (cube.zMax - lOz) / (dz == 0.0f ? 0.00001f : dz);
        float min1 = (t1 < t2) ? t1 : t2; float min2 = (t3 < t4) ? t3 : t4; float min3 = (t5 < t6) ? t5 : t6;
        float tmin = (min1 > min2) ? ((min1 > min3) ? min1 : min3) : ((min2 > min3) ? min2 : min3);
        float max1 = (t1 > t2) ? t1 : t2; float max2 = (t3 > t4) ? t3 : t4; float max3 = (t5 > t6) ? t5 : t6;
        float tmax = (max1 < max2) ? ((max1 < max3) ? max1 : max3) : ((max2 < max3) ? max2 : max3);
        if (tmax >= 0.0f && tmin <= tmax) {
            if (tmin < minT) { minT = tmin; closestIndex = (int)i; }
        }
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
    float speed = 4.0f * dt;
    if (activeGizmoAxis == GIZMO_NONE) {
        if (GetAsyncKeyState('W') & 0x8000) { camX += lookX * speed; camY += lookY * speed; camZ += lookZ * speed; }
        if (GetAsyncKeyState('S') & 0x8000) { camX -= lookX * speed; camY -= lookY * speed; camZ -= lookZ * speed; }
        if (GetAsyncKeyState('A') & 0x8000) { camX -= -lookZ * speed; camZ -= lookX * speed; }
        if (GetAsyncKeyState('D') & 0x8000) { camX += -lookZ * speed; camZ -= lookX * speed; }
        if (GetAsyncKeyState(VK_SPACE) & 0x8000) camY += speed;
        if (GetAsyncKeyState(VK_LSHIFT) & 0x8000) camY -= speed;
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

            // Рассчитываем новые габариты AABB для Gizmo и кликов
            if (activeGizmoAxis == GIZMO_X) { float nX = dragStartSizeX + deltaX * 2.0f; if (nX < 0.1f) nX = 0.1f; cube.xMin = -nX / 2.0f; cube.xMax = nX / 2.0f; }
            if (activeGizmoAxis == GIZMO_Y) { float nY = dragStartSizeY + deltaY * 2.0f; if (nY < 0.1f) nY = 0.1f; cube.yMin = -nY / 2.0f; cube.yMax = nY / 2.0f; }
            if (activeGizmoAxis == GIZMO_Z) { float nZ = dragStartSizeZ + (deltaX - deltaY) * 1.0f; if (nZ < 0.1f) nZ = 0.1f; cube.zMin = -nZ / 2.0f; cube.zMax = nZ / 2.0f; }

            // Если это кастомная модель, просто обновляем коэффициенты для видеокарты (без циклов!)
            if (cube.isCustomModel) {
                cube.scaleX = (cube.xMax - cube.xMin) / dragStartSizeX;
                cube.scaleY = (cube.yMax - cube.yMin) / dragStartSizeY;
                cube.scaleZ = (cube.zMax - cube.zMin) / dragStartSizeZ;
            }
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
        if (selectedCubeIndex != -1 && selectedCubeIndex < (int)sceneCubes.size() && sceneCubes[selectedCubeIndex].exists) {
            bool isLocked = sceneCubes[selectedCubeIndex].isLocked;
            AppendMenuW(hMenu, isLocked ? MF_GRAYED : MF_STRING, 1001, L"Удалить (Delete)");
            AppendMenuW(hMenu, isLocked ? MF_GRAYED : MF_STRING, 1002, L"Копировать (Copy)");
            AppendMenuW(hMenu, isLocked ? MF_GRAYED : MF_STRING, 1003, L"Вырезать (Cut)");
            AppendMenuW(hMenu, MF_STRING, 1005, isLocked ? L"Разблокировать (Unlock)" : L"Заблокировать (Lock)");
        }
        else { AppendMenuW(hMenu, hasClipboardData ? MF_STRING : MF_GRAYED, 1004, L"Вставить (Paste)"); }
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, p.x, p.y, 0, hwnd, nullptr); DestroyMenu(hMenu); break;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wp);
        if (wmId == 1006) { // ID_MENU_IMPORT_OBJ
            const wchar_t* filePath = Open3DFileDialog(hwnd);
            if (filePath != nullptr) {
                std::wstring selectedPath(filePath);
                std::wstring finalPath = selectedPath;
                bool is3MF = false;

                if (selectedPath.length() > 4 && selectedPath.substr(selectedPath.length() - 3) == L"3mf") {
                    is3MF = true;
                    std::wstring outputDir = selectedPath.substr(0, selectedPath.find_last_of(L'\\')) + L"\\_tmp_3mf";

                    // ИСПРАВЛЕНО: Передаем указатель .c_str() для системной Win32 функции
                    CreateDirectoryW(outputDir.c_str(), nullptr);

                    // ИСПРАВЛЕНО СИНТАКСИЧЕСКИ: Идеальные кавычки для tar.exe
                    std::wstring cmd = L"tar.exe -xf \"" + selectedPath + L"\" -C \"" + outputDir + L"\"";

                    STARTUPINFOW si = { sizeof(si) };
                    PROCESS_INFORMATION pi;
                    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;

                    if (CreateProcessW(nullptr, const_cast<wchar_t*>(cmd.c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
                        WaitForSingleObject(pi.hProcess, INFINITE);
                        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
                    }
                    finalPath = outputDir + L"\\3D\\3dmodel.model";
                }

                EditableCube newModel;
                // ИСПРАВЛЕНО: Передаем finalPath.c_str() в LoadOBJModel
                if (LoadOBJModel(finalPath.c_str(), newModel)) {
                    newModel.posX = camX + lookX * 3.0f;
                    newModel.posY = camY + lookY * 3.0f;
                    newModel.posZ = camZ + lookZ * 3.0f;
                    newModel.isSelected = false;
                    newModel.isLocked = false;
                    newModel.exists = true;
                    sceneCubes.push_back(newModel);
                }
                else {
                    MessageBoxW(hwnd, L"Не удалось распарсить геометрию файла.", L"Ошибка импорта", MB_OK | MB_ICONERROR);
                }

                if (is3MF) {
                    std::wstring outputDir = selectedPath.substr(0, selectedPath.find_last_of(L'\\')) + L"\\_tmp_3mf";
                    std::wstring modelFile = outputDir + L"\\3D\\3dmodel.model";
                    std::wstring dir3D = outputDir + L"\\3D";
                    DeleteFileW(modelFile.c_str());
                    RemoveDirectoryW(dir3D.c_str());
                    RemoveDirectoryW(outputDir.c_str());
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
    HMENU hTopMenu = CreateMenu();
    HMENU hFileSubMenu = CreatePopupMenu();
    AppendMenuW(hFileSubMenu, MF_STRING, 1006, L"Импортировать 3D файл (.obj/.3mf)...");
    AppendMenuW(hTopMenu, MF_POPUP, (UINT_PTR)hFileSubMenu, L"Файл");
    HWND hwnd = CreateWindowExW(0, myClassName, L"Blender Gizmo Editor Hybrid",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 800, 600,
        nullptr, hTopMenu, hInst, nullptr);
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
            if (triggerLeftClickPicking) {
                if (selectedCubeIndex != -1 && selectedCubeIndex < (int)sceneCubes.size() && sceneCubes[selectedCubeIndex].exists && !sceneCubes[selectedCubeIndex].isLocked) {
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
            if (triggerDoubleClick) {
                float ox, oy, oz, dx, dy, dz;
                ScreenToWorldRay(clickMouseX, clickMouseY, ox, oy, oz, dx, dy, dz);
                int clickedCube = GetCubeUnderMouseRay(ox, oy, oz, dx, dy, dz);
                if (clickedCube != -1 && clickedCube < (int)sceneCubes.size() && sceneCubes[clickedCube].exists) {
                    if (selectedCubeIndex == clickedCube) {
                        sceneCubes[selectedCubeIndex].isSelected = false;
                        selectedCubeIndex = -1;
                    }
                    else {
                        if (selectedCubeIndex != -1 && selectedCubeIndex < (int)sceneCubes.size()) sceneCubes[selectedCubeIndex].isSelected = false;
                        selectedCubeIndex = clickedCube;
                        sceneCubes[selectedCubeIndex].isSelected = true;
                    }
                }
                else {
                    if (selectedCubeIndex != -1 && selectedCubeIndex < (int)sceneCubes.size()) {
                        sceneCubes[selectedCubeIndex].isSelected = false;
                        selectedCubeIndex = -1;
                    }
                }
                triggerDoubleClick = false;
            }
            if (triggerRightClickPicking) {
                triggerRightClickPicking = false;
            }
            for (const auto& cube : sceneCubes) DrawEditableCube(cube);
            if (selectedCubeIndex != -1 && selectedCubeIndex < (int)sceneCubes.size() && sceneCubes[selectedCubeIndex].exists && !sceneCubes[selectedCubeIndex].isLocked) DrawGizmoArrows(sceneCubes[selectedCubeIndex]);
            glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, screenW, 0, screenH, -1, 1);
            glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glDisable(GL_DEPTH_TEST); glColor3f(1.0f, 1.0f, 1.0f);
            RenderText(15.0f, (float)screenH - 25.0f, uiText);
            RenderText(15.0f, 25.0f, "Double-LMB on Cube: Select (Orange Outline) | LMB Drag on Arrows: Resize Cube | RMB: Menu");
            if (selectedCubeIndex != -1 && selectedCubeIndex < (int)sceneCubes.size() && sceneCubes[selectedCubeIndex].exists) { std::string s = "Editing Cube #" + std::to_string(selectedCubeIndex); if (sceneCubes[selectedCubeIndex].isLocked) s += " [LOCKED]"; RenderText(15.0f, (float)screenH - 45.0f, s); }
            glEnable(GL_DEPTH_TEST); SwapBuffers(dc);
        }
    }
    wglMakeCurrent(nullptr, nullptr); wglDeleteContext(rc); ReleaseDC(hwnd, dc); return 0;
}
