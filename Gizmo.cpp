#include "Gizmo.h"
#include <gl/GLU.h> 

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
    glPushMatrix();
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f); 
    glColor3f(0.9f, 0.1f, 0.1f);
    gluCylinder(quad, stickRadius, stickRadius, stickLength, segments, 1);
    glTranslatef(0.0f, 0.0f, stickLength);
    gluCylinder(quad, coneRadius, 0.0f, coneLength, segments, 1);
    glPopMatrix();
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glColor3f(0.1f, 0.9f, 0.1f);
    gluCylinder(quad, stickRadius, stickRadius, stickLength, segments, 1);
    glTranslatef(0.0f, 0.0f, stickLength);
    gluCylinder(quad, coneRadius, 0.0f, coneLength, segments, 1);
    glPopMatrix();
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
