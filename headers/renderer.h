#ifndef RENDERER_H
#define RENDERER_H

#include "mesh.h"

class Renderer {
public:
    bool init(int width, int height, const char* title);
    void loadMesh(Mesh& mesh);
    void renderLoop();
    void cleanup();

private:
    void* window = nullptr;
    unsigned int VAO, VBO;
    unsigned int shaderProgram;
    int vertexCount = 0;
    float meshScale = 1.0f;
    float meshCenterX = 0, meshCenterY = 0, meshCenterZ = 0;

    unsigned int compileShader(unsigned int type, const char* source);
    unsigned int createShaderProgram();
};

#endif