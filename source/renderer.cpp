#include "renderer.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    uniform mat4 MVP;
    void main() {
        gl_Position = MVP * vec4(aPos, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    uniform vec4 color;
    void main() {
        FragColor = color;
    }
)";

// mouse state
static float yaw = 0.0f;
static float pitch = 0.0f;
static float zoom = 3.0f;
static bool mouseDown = false;
static double lastX = 0, lastY = 0;

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

    glfwSetMouseButtonCallback((GLFWwindow*)window, mouseButtonCallback);
    glfwSetCursorPosCallback((GLFWwindow*)window, cursorPosCallback);
    glfwSetScrollCallback((GLFWwindow*)window, scrollCallback);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD" << std::endl;
        return false;
    }

    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    shaderProgram = createShaderProgram();
    return true;
}

void Renderer::loadMesh(Mesh& mesh) {
    vertexCount = mesh.vertexBuffer.size() / 3;
    meshScale = mesh.scale;
    meshCenterX = mesh.center.x;
    meshCenterY = mesh.center.y;
    meshCenterZ = mesh.center.z;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertexBuffer.size() * sizeof(float),
                 mesh.vertexBuffer.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

void Renderer::renderLoop() {
    unsigned int colorLoc = glGetUniformLocation(shaderProgram, "color");

    while (!glfwWindowShouldClose((GLFWwindow*)window)) {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
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

        // draw filled triangles in green
        glUniform4f(colorLoc, 0.2f, 0.8f, 0.4f, 1.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);

        // draw edges in black, slightly offset so they appear on top
        glUniform4f(colorLoc, 0.0f, 0.0f, 0.0f, 1.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        glDisable(GL_POLYGON_OFFSET_LINE);

        // reset to fill mode
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        glfwSwapBuffers((GLFWwindow*)window);
        glfwPollEvents();
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