#ifndef MESH_H
#define MESH_H

#include <vector>
#include <string>
#include <array>
#include <map>

struct Node {
    float x, y, z;
};

struct Tet {
    int n[4]; // 4 node indices
};

struct Vec3 {
    float x, y, z;
};

class Mesh {
public:
    std::vector<Node> nodes;
    std::vector<Tet> tets;
    std::vector<float> vertexBuffer;  // outer faces for rendering
    Vec3 center = {0, 0, 0};
    float scale = 1.0f;

    bool loadMSH(const std::string& filepath);
    void extractSurface();
    void computeBounds();
};

#endif