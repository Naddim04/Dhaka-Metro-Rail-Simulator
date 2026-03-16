/*
 * PROJECT: ULTIMATE DHAKA METRO SIMULATOR (MRT LINE 6)
 * ----------------------------------------------------------------------
 * UPDATES IN THIS VERSION:
 * 1. FIXED TRACK VISIBILITY: Platform split into two sides so tracks are visible in the center.
 * 2. FIXED STOPPING POSITION: Train pulls forward correctly.
 * 3. DDA ALGORITHM: Black outlines on pillars.
 * 4. SUN/MOON: Visible position.
 * ----------------------------------------------------------------------
 */

#include <GL/glut.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <cstdlib>

// --- CONFIGURATION ---
#define PI 3.14159265
#define TRAIN_SPEED 0.15f

// --- GLOBAL VARIABLES ---
// Camera (The User's Eye)
float camX = 15.0f, camY = 15.0f, camZ = 40.0f;
float camYaw = -90.0f; // looking straight down the Z axis

// Environment
bool isDay = true;  // Press 'n' to toggle
bool isRain = false; // Press 'r' to toggle

// Train Logic
float trainZ = -120.0f;        // Train starts far behind
float doorGap = 0.0f;          // Gap for train doors opening
int trainState = 0;            // 0:Moving, 1:Stopping, 2:DoorsOpen, 3:DoorsClose, 4:Leaving
int waitTimer = 0;             // How long the train waits at station

// Station Platform Doors (PSD) Logic
float stationDoorGap = 0.0f;
const float STATION_DOOR_MAX_GAP = 0.9f;

// Barrier door LED logic (Blinking Light)
bool barrierLEDOn = false;
bool barrierLEDBlinkState = false;
int ledBlinkTimer = 0;

// Rain Particles
struct Particle {
    float x, y, z;
    float speed;
};
std::vector<Particle> rainDrops;

// --- PROTOTYPES ---
void drawCylinder(float radius, float height, float r, float g, float b);
void drawTree(float x, float z);
void drawBuilding(float x, float z, float h);
void initRain();
void drawSunMoon();
void drawPlatformScreenDoors();
void drawDDALine(float x1, float y1, float x2, float y2, float z); // Algorithm to draw lines

// ==================================================================
// MEMBER 1: ENVIRONMENT & CITY SCAPE
// ==================================================================
void drawEnvironment() {
    // 1. Infinite Ground (Green Grass)
    glColor3f(0.1f, 0.4f, 0.1f);
    glBegin(GL_QUADS);
        glVertex3f(-300, 0, 300);
        glVertex3f(300, 0, 300);
        glVertex3f(300, 0, -300);
        glVertex3f(-300, 0, -300);
    glEnd();

    // 2. Main Road (Black Asphalt)
    glColor3f(0.15f, 0.15f, 0.15f);
    glBegin(GL_QUADS);
        glVertex3f(-12, 0.1f, 300);
        glVertex3f(12, 0.1f, 300);
        glVertex3f(12, 0.1f, -300);
        glVertex3f(-12, 0.1f, -300);
    glEnd();

    // 3. Road Markings (White Lines)
    glColor3f(1.0f, 1.0f, 1.0f);
    for(int z = -280; z < 280; z+=15) {
        glBegin(GL_QUADS);
            glVertex3f(-0.5f, 0.2f, z + 4.0f);
            glVertex3f(0.5f, 0.2f, z + 4.0f);
            glVertex3f(0.5f, 0.2f, z - 4.0f);
            glVertex3f(-0.5f, 0.2f, z - 4.0f);
        glEnd();
    }

    // 4. Procedural Buildings & Trees (Loop to create many objects)
    for(int z = -150; z < 150; z+=30) {
        // Left side
        drawBuilding(-40.0f, (float)z, 15.0f + (abs(z)%12));
        drawTree(-25.0f, (float)z + 10.0f);

        // Right side
        drawBuilding(40.0f, (float)z, 22.0f + (abs(z)%8));
        drawTree(25.0f, (float)z - 10.0f);
    }
}

void drawBuilding(float x, float z, float h) {
    // Building Body (Pastel Colors)
    glColor3f(0.5f + (sin(x)/4), 0.5f + (cos(z)/4), 0.6f);
    glPushMatrix();
        glTranslatef(x, h/2, z);
        glScalef(12.0f, h, 12.0f);
        glutSolidCube(1.0f);
    glPopMatrix();

    // Windows
    glPushMatrix();
        // Day: Dark Blue Windows | Night: Yellow Light Windows
        if(isDay) glColor3f(0.2f, 0.3f, 0.5f);
        else glColor3f(1.0f, 1.0f, 0.0f);

        glTranslatef(x, h/2, z + 6.1f);
        glScalef(8.0f, h-2.0f, 0.1f);
        glutSolidCube(1.0f);
    glPopMatrix();
}

void drawTree(float x, float z) {
    glPushMatrix();
        glTranslatef(x, 0, z);
        glRotatef(-90, 1, 0, 0);
        drawCylinder(0.6f, 5.0f, 0.4f, 0.2f, 0.0f); // Brown Trunk
        glTranslatef(0.0f, 0.0f, 4.0f);
        glColor3f(1.0f, 0.2f, 0.0f); // Orange Leaves (Krishnochura)
        glutSolidCone(3.5f, 6.0f, 10, 10);
    glPopMatrix();
}

// ==================================================================
// DDA ALGORITHM IMPLEMENTATION (Digital Differential Analyzer)
// ==================================================================
// This function calculates points to draw a straight line between two coordinates.
// We use this to draw the black outlines on the pillars.
void drawDDALine(float x1, float y1, float x2, float y2, float z) {
    float dx = x2 - x1;
    float dy = y2 - y1;

    // Find which difference is bigger to decide the number of steps
    float steps = (abs(dx) > abs(dy)) ? abs(dx) : abs(dy);

    if (steps == 0) return; // Prevent division by zero

    float xInc = dx / steps; // How much X changes per step
    float yInc = dy / steps; // How much Y changes per step

    float x = x1;
    float y = y1;

    glBegin(GL_POINTS);
    // Loop to draw dots along the line
    for(int i = 0; i <= steps * 10; i++) { // Multiply by 10 for smoother line
        glVertex3f(x, y, z);
        x += (xInc / 10.0f);
        y += (yInc / 10.0f);
    }
    glEnd();
}

// ==================================================================
// MEMBER 2: VIADUCTS & STATION
// ==================================================================
void drawStation() {
    // 1. Viaduct (The Bridge)
    glColor3f(0.9f, 0.9f, 0.9f); // Concrete Color
    glPushMatrix();
        glTranslatef(0.0f, 10.0f, 0.0f);
        glScalef(8.0f, 1.0f, 400.0f);
        glutSolidCube(1.0f);
    glPopMatrix();

    // Rails
    float railY = 10.6f;
    glColor3f(0.25f, 0.25f, 0.25f); // Dark Grey Rails
    glBegin(GL_QUADS);
        glVertex3f(-1.0f, railY, 200.0f); glVertex3f(-0.7f, railY, 200.0f);
        glVertex3f(-0.7f, railY, -200.0f); glVertex3f(-1.0f, railY, -200.0f);
    glEnd();
    glBegin(GL_QUADS);
        glVertex3f(0.7f, railY, 200.0f); glVertex3f(1.0f, railY, 200.0f);
        glVertex3f(1.0f, railY, -200.0f); glVertex3f(0.7f, railY, -200.0f);
    glEnd();

    // Sleepers (Wood under rails)
    glColor3f(0.35f, 0.35f, 0.35f);
    for(int z = -190; z <= 190; z += 5) {
        glBegin(GL_QUADS);
            glVertex3f(-2.0f, railY - 0.02f, z + 0.5f);
            glVertex3f(2.0f,  railY - 0.02f, z + 0.5f);
            glVertex3f(2.0f,  railY - 0.02f, z - 0.5f);
            glVertex3f(-2.0f, railY - 0.02f, z - 0.5f);
        glEnd();
    }

    // 2. Pillars WITH DDA OUTLINES
    for(int z = -150; z <= 150; z+=30) {
        // Draw the 3D Cylinder Pillar
        glPushMatrix();
            glTranslatef(0.0f, 0.0f, z);
            glRotatef(-90, 1, 0, 0);
            drawCylinder(2.0f, 10.0f, 0.7f, 0.7f, 0.7f);
        glPopMatrix();

        // Draw Black Outlines using DDA Algorithm
        glColor3f(0.0f, 0.0f, 0.0f); // Black Color

        // Left Line
        drawDDALine(-2.0f, 0.0f, -2.0f, 10.0f, (float)z);
        // Right Line
        drawDDALine(2.0f, 0.0f, 2.0f, 10.0f, (float)z);
    }

    // 3. Station Platform (SPLIT INTO TWO TO REVEAL TRACKS)
    glColor3f(0.95f, 0.95f, 1.0f);

    // Left Platform Block
    glPushMatrix();
        glTranslatef(-5.0f, 10.5f, 0.0f); // Shift left
        glScalef(5.0f, 0.5f, 60.0f);      // Width 5
        glutSolidCube(1.0f);
    glPopMatrix();

    // Right Platform Block
    glPushMatrix();
        glTranslatef(5.0f, 10.5f, 0.0f);  // Shift right
        glScalef(5.0f, 0.5f, 60.0f);      // Width 5
        glutSolidCube(1.0f);
    glPopMatrix();

    // The gap between -2.5 and +2.5 now reveals the tracks!

    // 4. Roof
    glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
    glPushMatrix();
        glTranslatef(0.0f, 18.0f, 0.0f);
        glScalef(1.0f, 0.3f, 1.0f);
        glRotatef(90, 0, 0, 1);
        drawCylinder(12.0f, 60.0f, 1.0f, 1.0f, 1.0f);
    glPopMatrix();

    drawPlatformScreenDoors();
}

void drawPlatformScreenDoors() {
    float platformY = 10.5f;
    float doorHeight = 2.0f;

    for (int side = -1; side <= 1; side += 2) {
        float xCenter = side * 3.5f;
        for (float z = -25.0f; z <= 25.0f; z += 3.0f) {
            // Frame
            glColor3f(0.6f, 0.6f, 0.65f);
            glPushMatrix();
                glTranslatef(xCenter, platformY + doorHeight/2, z);
                glScalef(0.1f, doorHeight, 0.1f);
                glutSolidCube(1.0f);
            glPopMatrix();

            // Sliding Leaves (Syncs with stationDoorGap)
            glColor3f(0.8f, 0.8f, 0.85f);
            // Leaf 1
            glPushMatrix();
                glTranslatef(xCenter, platformY + doorHeight/2, z - 0.8f - stationDoorGap);
                glScalef(0.1f, doorHeight, 1.5f);
                glutSolidCube(1.0f);
            glPopMatrix();
            // Leaf 2
            glPushMatrix();
                glTranslatef(xCenter, platformY + doorHeight/2, z + 0.8f + stationDoorGap);
                glScalef(0.1f, doorHeight, 1.5f);
                glutSolidCube(1.0f);
            glPopMatrix();

            // LED Light on top
            glPushMatrix();
                if (barrierLEDOn && barrierLEDBlinkState) glColor3f(0.0f, 1.0f, 0.0f); // Green ON
                else glColor3f(0.1f, 0.1f, 0.1f); // OFF
                glTranslatef(xCenter, platformY + doorHeight + 0.2f, z);
                glutSolidSphere(0.1f, 8, 8);
            glPopMatrix();
        }
    }
}

// ==================================================================
// MEMBER 3: THE TRAIN
// ==================================================================
void drawTrainCar(bool isEngine) {
    glPushMatrix();
    // Body
    glColor3f(0.8f, 0.8f, 0.8f); // Silver
    glScalef(1.5f, 1.5f, 8.0f);
    glutSolidCube(2.0f);
    glPopMatrix();

    // Green Stripe
    glColor3f(0.0f, 0.6f, 0.2f);
    glPushMatrix();
        glTranslatef(0.0f, 0.5f, 0.0f);
        glScalef(1.55f, 0.4f, 8.0f);
        glutSolidCube(2.0f);
    glPopMatrix();

    // Doors
    glColor3f(0.3f, 0.3f, 0.3f);
    glPushMatrix();
        glTranslatef(1.51f, -0.2f, 2.0f + doorGap);
        glScalef(0.1f, 1.2f, 1.0f);
        glutSolidCube(1.0f);
    glPopMatrix();
    glPushMatrix();
        glTranslatef(1.51f, -0.2f, -2.0f - doorGap);
        glScalef(0.1f, 1.2f, 1.0f);
        glutSolidCube(1.0f);
    glPopMatrix();

    // Engine Nose (Front part)
    if(isEngine) {
        glPushMatrix();
            glTranslatef(0.0f, -0.5f, 8.0f);
            glColor3f(0.8f, 0.8f, 0.8f);
            glutSolidSphere(1.4f, 20, 20);
        glPopMatrix();
    }
}

void drawFullTrain() {
    glPushMatrix();
        glTranslatef(0.0f, 12.5f, trainZ);

        // 1. Engine
        drawTrainCar(true);
        // 2. Middle Car
        glTranslatef(0.0f, 0.0f, -16.5f);
        drawTrainCar(false);
        // 3. Rear Car
        glTranslatef(0.0f, 0.0f, -16.5f);
        drawTrainCar(false);
    glPopMatrix();
}

// ==================================================================
// MEMBER 4: WEATHER (SUN/MOON & RAIN)
// ==================================================================
void initRain() {
    for(int i=0; i<500; i++) {
        Particle p;
        p.x = (rand()%100) - 50;
        p.y = (rand()%50) + 10;
        p.z = (rand()%100) - 50;
        p.speed = 0.5f + ((rand()%10)/10.0f);
        rainDrops.push_back(p);
    }
}

void drawRain() {
    if(!isRain) return;
    glColor3f(0.6f, 0.7f, 1.0f);
    glBegin(GL_LINES);
    for(int i=0; i<rainDrops.size(); i++) {
        glVertex3f(rainDrops[i].x, rainDrops[i].y, rainDrops[i].z);
        glVertex3f(rainDrops[i].x, rainDrops[i].y - 0.5f, rainDrops[i].z);
        rainDrops[i].y -= rainDrops[i].speed;
        if(rainDrops[i].y < 0) rainDrops[i].y = 50.0f;
    }
    glEnd();
}

void drawSunMoon() {
    glPushMatrix();
        glDisable(GL_LIGHTING);

        // POSITION: Up high (y=60) and slightly forward (z=-80)
        // This ensures it is visible above the track.
        glTranslatef(0.0f, 60.0f, -80.0f);

        if (isDay) {
            // SUN: Yellow Sphere
            glColor3f(1.0f, 0.9f, 0.3f);
            glutSolidSphere(8.0f, 40, 40);
        } else {
            // MOON: White Sphere
            glColor3f(0.9f, 0.9f, 1.0f);
            glutSolidSphere(6.0f, 40, 40);
        }

        glEnable(GL_LIGHTING);
    glPopMatrix();
}

void setupLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);

    if(isDay) {
        GLfloat lightPos[] = { 0.0f, 100.0f, 0.0f, 1.0f };
        GLfloat diff[] = {1.0f, 1.0f, 0.9f, 1.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diff);
        glEnable(GL_LIGHT0);
        glDisable(GL_LIGHT1);

        // Day Sky Color (Light Blue)
        glClearColor(0.6f, 0.8f, 1.0f, 1.0f);
    } else {
        GLfloat lightPos[] = { 0.0f, 100.0f, 0.0f, 1.0f };
        GLfloat diff[] = {0.8f, 0.8f, 1.0f, 1.0f};
        glLightfv(GL_LIGHT1, GL_POSITION, lightPos);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, diff);
        glEnable(GL_LIGHT1);
        glDisable(GL_LIGHT0);

        // Night Sky Color (Dark Blue)
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    }
}

// ==================================================================
// MEMBER 5: LOGIC
// ==================================================================
void update() {
    switch(trainState) {
        case 0: // MOVING STATE
            trainZ += TRAIN_SPEED;

            // --- FIX FOR STOPPING POSITION ---
            // If we stop at 0.0, the engine is at the center, but the rest
            // of the train is behind. We must stop at 18.0f.
            // This pushes the train forward so the Middle Car is at the station.
            if(trainZ >= 18.0f) {
                trainZ = 18.0f;   // Snap to this position
                trainState = 1;   // Change state to STOPPING
            }
            break;

        case 1: // STOPPING STATE
            trainState = 2; // Move to Doors Opening immediately
            waitTimer = 0;
            break;

        case 2: // DOORS OPENING STATE
            if(doorGap < 1.5f) {
                doorGap += 0.02f; // Open Train Doors
            } else {
                waitTimer++;
                // Wait for some time (e.g. 300 cycles) before closing
                if(waitTimer > 300) trainState = 3;
            }
            break;

        case 3: // DOORS CLOSING STATE
            if(doorGap > 0.0f) {
                doorGap -= 0.02f; // Close Train Doors
            } else {
                trainState = 4; // Ready to Leave
            }
            break;

        case 4: // LEAVING STATE
            trainZ += TRAIN_SPEED;
            // Reset position to loop the animation
            if(trainZ > 150.0f) {
                trainZ = -150.0f;
                trainState = 0;
            }
            break;
    }

    // --- FIX FOR STATION DOORS ---
    // Synchronize Station Doors with Train State
    if (trainState == 2) {
        // If Train is stopped and waiting, Open Station Doors
        if (stationDoorGap < STATION_DOOR_MAX_GAP)
            stationDoorGap += 0.03f;
    } else {
        // If Train is moving, Keep Station Doors Closed
        if (stationDoorGap > 0.0f)
            stationDoorGap -= 0.03f;
    }

    // Blink the LED Light when doors are open
    if (trainState == 2) {
        barrierLEDOn = true;
        ledBlinkTimer++;
        if (ledBlinkTimer > 15) {
            barrierLEDBlinkState = !barrierLEDBlinkState;
            ledBlinkTimer = 0;
        }
    } else {
        barrierLEDOn = false;
        barrierLEDBlinkState = false;
    }

    glutPostRedisplay();
}

void handleKeys(unsigned char key, int x, int y) {
    float radianYaw = camYaw * PI / 180.0f;
    float speed = 1.5f;

    switch(key) {
        case 'w': camX += cos(radianYaw) * speed; camZ += sin(radianYaw) * speed; break;
        case 's': camX -= cos(radianYaw) * speed; camZ -= sin(radianYaw) * speed; break;
        case 'a': camX += cos(radianYaw - PI/2) * speed; camZ += sin(radianYaw - PI/2) * speed; break;
        case 'd': camX += cos(radianYaw + PI/2) * speed; camZ += sin(radianYaw + PI/2) * speed; break;
        case 'r': isRain = !isRain; break;
        case 'n': isDay = !isDay; break;
        case 27: exit(0); break;
    }
    glutPostRedisplay();
}

void handleMouse(int x, int y) {
    static int lastX = x;
    int diffX = x - lastX;
    lastX = x;
    camYaw += diffX * 0.2f;
    glutPostRedisplay();
}

void drawCylinder(float radius, float height, float r, float g, float b) {
    glColor3f(r, g, b);
    GLUquadric *quad = gluNewQuadric();
    gluCylinder(quad, radius, radius, height, 20, 20);
}

void drawInstructions() {
    glDisable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 800, 0, 600);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
        glColor3f(1, 0, 0);
        glRasterPos2i(10, 580);
        std::string s = "DIU METRO PROJECT | W,A,S,D to Walk | Mouse to Look | 'R' Rain | 'N' Day/Night";
        for (int i = 0; i < s.length(); i++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, s[i]);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_LIGHTING);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Camera setup
    float radianYaw = camYaw * PI / 180.0f;
    float targetX = camX + cos(radianYaw);
    float targetZ = camZ + sin(radianYaw);
    gluLookAt(camX, camY, camZ, targetX, camY, targetZ, 0, 1, 0);

    setupLighting();
    drawSunMoon();      // Draw the sky object
    drawEnvironment();  // Draw city and ground
    drawStation();      // Draw station structure
    drawFullTrain();    // Draw the train
    drawRain();         // Draw rain if enabled
    drawInstructions(); // Draw text on screen

    glutSwapBuffers();
}

void reshape(int w, int h) {
    if(h==0) h=1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (float)w / h, 1.0, 500.0);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1000, 600);
    glutCreateWindow("Realistic Dhaka Metro - DDA Version");

    glEnable(GL_DEPTH_TEST);
    initRain();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(update);
    glutKeyboardFunc(handleKeys);
    glutPassiveMotionFunc(handleMouse);

    glutMainLoop();
    return 0;
}
