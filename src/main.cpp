#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "FluidGrid.hpp"
#include "Shader.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

constexpr int WINDOW_WIDTH = 900;
constexpr int WINDOW_HEIGHT = 900;
constexpr int GRID_SIZE = 160;

enum class ColorPalette { Rainbow, Fire, Ocean };

static void HSVtoRGB(float h, float s, float v, float& r, float& g, float& b) {
    float c = v * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    if (h < 60.0f) {
        r = c;
        g = x;
        b = 0.0f;
    } else if (h < 120.0f) {
        r = x;
        g = c;
        b = 0.0f;
    } else if (h < 180.0f) {
        r = 0.0f;
        g = c;
        b = x;
    } else if (h < 240.0f) {
        r = 0.0f;
        g = x;
        b = c;
    } else if (h < 300.0f) {
        r = x;
        g = 0.0f;
        b = c;
    } else {
        r = c;
        g = 0.0f;
        b = x;
    }
    r += m;
    g += m;
    b += m;
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window =
        glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Tiny Fluid Simulator", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable VSync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glfwSetFramebufferSizeCallback(window,
                                   [](GLFWwindow*, int w, int h) { glViewport(0, 0, w, h); });

    Shader shader(SHADER_DIR "/fluid.vs", SHADER_DIR "/fluid.fs");

    // Fullscreen Quad (Position X, Y, TexCoord U, V)
    float quadVertices[] = {
        // Positions   // TexCoords
        -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,

        -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f};

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // Create OpenGL texture for streaming fluid grid
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    std::vector<uint8_t> pixelBuffer(GRID_SIZE * GRID_SIZE * 3, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GRID_SIZE, GRID_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 pixelBuffer.data());

    FluidGrid fluid(GRID_SIZE, 0.000001f, 0.000001f);

    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool firstMouse = true;

    float currentHue = 0.0f;
    ColorPalette palette = ColorPalette::Rainbow;
    bool showVelocity = false;
    int brushRadius = 5;

    double lastTime = glfwGetTime();

    std::cout << "========================================\n"
              << "   Tiny 2D Fluid Simulator\n"
              << "========================================\n"
              << " Left Click + Drag : Inject velocity and dye\n"
              << " Right Click + Drag: Stir velocity only\n"
              << " Key 'C' / Space   : Clear fluid\n"
              << " Key 'V'           : Toggle velocity visualization\n"
              << " Key '1'           : Rainbow hue cycling\n"
              << " Key '2'           : Fire & magma palette\n"
              << " Key '3'           : Electric cyan & ocean palette\n"
              << " Up / Down arrows  : Adjust brush radius\n"
              << " ESC               : Exit\n"
              << "========================================\n";

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;
        deltaTime = std::min(deltaTime, 0.033f);

        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Key controls
        static bool cPressed = false;
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            if (!cPressed) {
                fluid.reset();
                cPressed = true;
            }
        } else {
            cPressed = false;
        }

        static bool vPressed = false;
        if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
            if (!vPressed) {
                showVelocity = !showVelocity;
                vPressed = true;
            }
        } else {
            vPressed = false;
        }

        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
            palette = ColorPalette::Rainbow;
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
            palette = ColorPalette::Fire;
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
            palette = ColorPalette::Ocean;

        static bool upPressed = false;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            if (!upPressed) {
                brushRadius = std::min(15, brushRadius + 1);
                upPressed = true;
            }
        } else {
            upPressed = false;
        }

        static bool downPressed = false;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            if (!downPressed) {
                brushRadius = std::max(2, brushRadius - 1);
                downPressed = true;
            }
        } else {
            downPressed = false;
        }

        // Mouse input
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        int winWidth, winHeight;
        glfwGetWindowSize(window, &winWidth, &winHeight);

        if (firstMouse) {
            lastMouseX = mouseX;
            lastMouseY = mouseY;
            firstMouse = false;
        }

        double dx = mouseX - lastMouseX;
        double dy = mouseY - lastMouseY;
        lastMouseX = mouseX;
        lastMouseY = mouseY;

        bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        bool rightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

        if (winWidth > 0 && winHeight > 0 && (leftDown || rightDown)) {
            // Map window coordinates to fluid grid cell (1 to GRID_SIZE)
            int gx = static_cast<int>((mouseX / winWidth) * GRID_SIZE) + 1;
            int gy = static_cast<int>(((winHeight - mouseY) / winHeight) * GRID_SIZE) + 1;

            float vx = static_cast<float>(dx) * 2.5f;
            float vy = -static_cast<float>(dy) * 2.5f;

            fluid.addVelocity(gx, gy, vx, vy, brushRadius);

            if (leftDown) {
                float r = 1.0f, g = 1.0f, b = 1.0f;
                if (palette == ColorPalette::Rainbow) {
                    currentHue = std::fmod(currentHue + 1.5f, 360.0f);
                    HSVtoRGB(currentHue, 0.95f, 1.0f, r, g, b);
                } else if (palette == ColorPalette::Fire) {
                    r = 1.0f;
                    g = 0.35f + 0.3f * std::sin(currentTime * 4.0);
                    b = 0.05f;
                } else if (palette == ColorPalette::Ocean) {
                    r = 0.05f;
                    g = 0.65f + 0.35f * std::cos(currentTime * 3.0);
                    b = 1.0f;
                }
                fluid.addDensity(gx, gy, r * 1.5f, g * 1.5f, b * 1.5f, brushRadius);
            }
        }

        // Step simulation
        fluid.step(0.016f);

        // Extract pixel data and stream to GPU
        fluid.getPixels(pixelBuffer, showVelocity);

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GRID_SIZE, GRID_SIZE, GL_RGB, GL_UNSIGNED_BYTE,
                        pixelBuffer.data());

        // Render quad
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        shader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        shader.setInt("u_Texture", 0);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
    }

    glDeleteTextures(1, &textureID);
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
