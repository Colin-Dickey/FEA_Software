#ifndef RENDERER_H
#define RENDERER_H

#include "mesh.h"
#include <vector>

class Renderer {
public:
    bool init(int width, int height, const char* title);
    void loadMesh(Mesh& mesh, const std::vector<double>& vonMises);
    void renderLoop();
    void cleanup();
    std::vector<double>* displacementsPtr = nullptr;
    
private:
    void* window = nullptr;
    unsigned int VAO, VBO;
    unsigned int shaderProgram;
    int vertexCount = 0;
    float meshScale = 1.0f;
    float meshCenterX = 0, meshCenterY = 0, meshCenterZ = 0;

    // deformation
    Mesh* meshPtr = nullptr;
    const std::vector<double>* vonMisesPtr = nullptr;

    float deformScale = 0.0f;

    void rebuildBuffer();

    unsigned int compileShader(unsigned int type, const char* source);
    unsigned int createShaderProgram();
};

#endif