#include "renderer.h"
#include "solver.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <map>

const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aColor;
    uniform mat4 MVP;
    out vec3 fragColor;
    void main() {
        gl_Position = MVP * vec4(aPos, 1.0);
        fragColor = aColor;
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    in vec3 fragColor;
    out vec4 FragColor;
    uniform bool wireframe;
    void main() {
        if (wireframe)
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        else
            FragColor = vec4(fragColor, 1.0);
    }
)";

// mouse state
static float yaw = 0.0f;
static float pitch = 0.0f;
static float zoom = 3.0f;
static bool mouseDown = false;
static double lastX = 0, lastY = 0;

// deformation scale
static float deformScale = 0.0f;
static bool needsRebuild = false;

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        mouseDown = (action == GLFW_PRESS);
        if (mouseDown) glfwGetCursorPos(window, &lastX, &lastY);
    }
}

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    if (!mouseDown) return;
    float dx = (float)(xpos - lastX);
    float dy = (float)(ypos - lastY);
    lastX = xpos;
    lastY = ypos;
    yaw   += dx * 0.4f;
    pitch += dy * 0.4f;
    if (pitch >  89.0f) pitch =  89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    zoom -= (float)yoffset * 0.2f;
    if (zoom < 0.5f) zoom = 0.5f;
    if (zoom > 20.0f) zoom = 20.0f;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD) {
            if (deformScale == 0.0f)
                deformScale = 1.0f;
            else
                deformScale *= 2.0f;
            needsRebuild = true;
            std::cout << "Deformation scale: " << deformScale << "x" << std::endl;
        }
        if (key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) {
            deformScale /= 2.0f;
            if (deformScale < 0.5f) deformScale = 0.0f;
            needsRebuild = true;
            std::cout << "Deformation scale: " << deformScale << "x" << std::endl;
        }
        if (key == GLFW_KEY_0) {
            deformScale = 0.0f;
            needsRebuild = true;
            std::cout << "Deformation scale: 0 (undeformed)" << std::endl;
        }
    }
}

static glm::vec3 stressColour(float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    float r, g, b;
    if (t < 0.25f) {
        r = 0.0f; g = t / 0.25f; b = 1.0f;
    } else if (t < 0.5f) {
        r = 0.0f; g = 1.0f; b = 1.0f - (t - 0.25f) / 0.25f;
    } else if (t < 0.75f) {
        r = (t - 0.5f) / 0.25f; g = 1.0f; b = 0.0f;
    } else {
        r = 1.0f; g = 1.0f - (t - 0.75f) / 0.25f; b = 0.0f;
    }
    return {r, g, b};
}

bool Renderer::init(int width, int height, const char* title) {
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW" << std::endl;
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        return false;
    }
    glfwMakeContextCurrent((GLFWwindow*)window);
    glfwSwapInterval(1);

    glfwSetMouseButtonCallback((GLFWwindow*)window, mouseButtonCallback);
    glfwSetCursorPosCallback((GLFWwindow*)window, cursorPosCallback);
    glfwSetScrollCallback((GLFWwindow*)window, scrollCallback);
    glfwSetKeyCallback((GLFWwindow*)window, keyCallback);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD" << std::endl;
        return false;
    }

    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    shaderProgram = createShaderProgram();
    return true;
}

void Renderer::loadMesh(Mesh& mesh, const std::vector<double>& vonMises) {
    meshPtr      = &mesh;
    vonMisesPtr  = &vonMises;
    meshScale    = mesh.scale;
    meshCenterX  = mesh.center.x;
    meshCenterY  = mesh.center.y;
    meshCenterZ  = mesh.center.z;
    deformScale  = 0.0f;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    rebuildBuffer();

    std::cout << "Controls: +/- to scale deformation, 0 to reset, scroll to zoom, drag to rotate" << std::endl;
}

void Renderer::rebuildBuffer() {
    Mesh& mesh = *meshPtr;
    const std::vector<double>& vonMises = *vonMisesPtr;

    double minVM = 0.0;  // always start colour scale at zero
    double maxVM = *std::max_element(vonMises.begin(), vonMises.end());
    double range = maxVM - minVM;
    if (range < 1e-10) range = 1.0;

    std::map<std::array<int,3>, int> faceCount;
    auto makeFaceKey = [](int a, int b, int c) {
        std::array<int,3> f = {a, b, c};
        std::sort(f.begin(), f.end());
        return f;
    };

    for (auto& tet : mesh.tets) {
        faceCount[makeFaceKey(tet.n[0], tet.n[1], tet.n[2])]++;
        faceCount[makeFaceKey(tet.n[0], tet.n[1], tet.n[3])]++;
        faceCount[makeFaceKey(tet.n[0], tet.n[2], tet.n[3])]++;
        faceCount[makeFaceKey(tet.n[1], tet.n[2], tet.n[3])]++;
    }

    std::vector<float> buffer;
    for (auto& tet : mesh.tets) {
        std::array<std::array<int,3>, 4> faces = {{
            {tet.n[0], tet.n[1], tet.n[2]},
            {tet.n[0], tet.n[1], tet.n[3]},
            {tet.n[0], tet.n[2], tet.n[3]},
            {tet.n[1], tet.n[2], tet.n[3]}
        }};

        for (auto& f : faces) {
            auto key = makeFaceKey(f[0], f[1], f[2]);
            if (faceCount[key] == 1) {
                for (int idx : f) {
                    auto& n = mesh.nodes[idx];

                    // apply deformation
                    float x = n.x;
                    float y = n.y;
                    float z = n.z;

                    if (displacementsPtr && displacementsPtr->size() > (size_t)(idx*3+2)) {
                        x += ::deformScale * (*displacementsPtr)[idx*3+0];
                        y += ::deformScale * (*displacementsPtr)[idx*3+1];
                        z += ::deformScale * (*displacementsPtr)[idx*3+2];
                    }

                    buffer.push_back(x);
                    buffer.push_back(y);
                    buffer.push_back(z);

                    float t = (float)((vonMises[idx] - minVM) / range);
                    glm::vec3 col = stressColour(t);
                    buffer.push_back(col.r);
                    buffer.push_back(col.g);
                    buffer.push_back(col.b);
                }
            }
        }
    }

    vertexCount = buffer.size() / 6;

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, buffer.size() * sizeof(float),
                 buffer.data(), GL_DYNAMIC_DRAW);
}

void Renderer::renderLoop() {
    unsigned int wireframeLoc = glGetUniformLocation(shaderProgram, "wireframe");

    while (!glfwWindowShouldClose((GLFWwindow*)window)) {
        double frameStart = glfwGetTime();

        if (needsRebuild) {
            rebuildBuffer();
            needsRebuild = false;
        }

        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, glm::radians(pitch), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(yaw),   glm::vec3(0, 1, 0));
        model = glm::scale(model, glm::vec3(meshScale));
        model = glm::translate(model, glm::vec3(-meshCenterX, -meshCenterY, -meshCenterZ));

        glm::mat4 view = glm::lookAt(
            glm::vec3(0, 0, zoom),
            glm::vec3(0, 0, 0),
            glm::vec3(0, 1, 0)
        );
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), 800.0f/600.0f, 0.1f, 1000.0f);
        glm::mat4 MVP = proj * view * model;

        unsigned int mvpLoc = glGetUniformLocation(shaderProgram, "MVP");
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(MVP));

        glBindVertexArray(VAO);

        glUniform1i(wireframeLoc, 0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);

        glUniform1i(wireframeLoc, 1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        glDisable(GL_POLYGON_OFFSET_LINE);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        glfwSwapBuffers((GLFWwindow*)window);

        double frameTime = glfwGetTime() - frameStart;
        if (frameTime < 1.0/60.0) {
            glfwWaitEventsTimeout((1.0/60.0) - frameTime);
        } else {
            glfwPollEvents();
        }
    }
}

void Renderer::cleanup() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
}

unsigned int Renderer::compileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

unsigned int Renderer::createShaderProgram() {
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}