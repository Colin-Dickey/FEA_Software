#ifndef MESH_H
#define MESH_H

#include <vector>
#include <string>
#include <array>

struct Vertex {
    float x, y, z;
};

struct Triangle {
    std::array<Vertex, 3> vertices;
};

struct Vec3 {
    float x, y, z;
};

class Mesh {
public:
    std::vector<Triangle> triangles;
    std::vector<float> vertexBuffer;
    Vec3 center = {0, 0, 0};
    float scale = 1.0f;

    bool loadSTL(const std::string& filepath);
    void buildVertexBuffer();
    void computeBounds();
};

#endif