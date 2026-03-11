#include "mesh.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <limits>

bool Mesh::loadSTL(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "Could not open file: " << filepath << std::endl;
        return false;
    }

    char header[80];
    file.read(header, 80);

    uint32_t numTriangles;
    file.read(reinterpret_cast<char*>(&numTriangles), 4);
    std::cout << "Loading " << numTriangles << " triangles..." << std::endl;

    triangles.resize(numTriangles);

    for (uint32_t i = 0; i < numTriangles; i++) {
        float normal[3];
        file.read(reinterpret_cast<char*>(normal), 12);

        for (int j = 0; j < 3; j++) {
            float v[3];
            file.read(reinterpret_cast<char*>(v), 12);
            triangles[i].vertices[j] = {v[0], v[1], v[2]};
        }

        uint16_t attrib;
        file.read(reinterpret_cast<char*>(&attrib), 2);
    }

    buildVertexBuffer();
    computeBounds();
    return true;
}

void Mesh::buildVertexBuffer() {
    vertexBuffer.clear();
    for (auto& tri : triangles) {
        for (auto& v : tri.vertices) {
            vertexBuffer.push_back(v.x);
            vertexBuffer.push_back(v.y);
            vertexBuffer.push_back(v.z);
        }
    }
}

void Mesh::computeBounds() {
    float minX, minY, minZ, maxX, maxY, maxZ;
    minX = minY = minZ = std::numeric_limits<float>::max();
    maxX = maxY = maxZ = std::numeric_limits<float>::lowest();

    for (auto& tri : triangles) {
        for (auto& v : tri.vertices) {
            minX = std::min(minX, v.x); maxX = std::max(maxX, v.x);
            minY = std::min(minY, v.y); maxY = std::max(maxY, v.y);
            minZ = std::min(minZ, v.z); maxZ = std::max(maxZ, v.z);
        }
    }

    center = {(minX + maxX) / 2, (minY + maxY) / 2, (minZ + maxZ) / 2};
    float sizeX = maxX - minX;
    float sizeY = maxY - minY;
    float sizeZ = maxZ - minZ;
    scale = 2.0f / std::max(sizeX, std::max(sizeY, sizeZ));

    std::cout << "Center: " << center.x << ", " << center.y << ", " << center.z << std::endl;
    std::cout << "Scale: " << scale << std::endl;
}