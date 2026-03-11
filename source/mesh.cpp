#include "mesh.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <limits>
#include <set>
#include <algorithm>

bool Mesh::loadMSH(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file) {
        std::cerr << "Could not open file: " << filepath << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {

        // parse nodes
        if (line == "$Nodes") {
            int numNodes;
            file >> numNodes;
            nodes.resize(numNodes);
            for (int i = 0; i < numNodes; i++) {
                int id;
                float x, y, z;
                file >> id >> x >> y >> z;
                nodes[id - 1] = {x, y, z};
            }
        }

        // parse elements
        if (line == "$Elements") {
            int numElements;
            file >> numElements;
            for (int i = 0; i < numElements; i++) {
                int id, type, numTags;
                file >> id >> type >> numTags;

                // skip tags
                for (int t = 0; t < numTags; t++) {
                    int tag; file >> tag;
                }

                if (type == 4) {
                    // linear tet = element type 4 in Gmsh
                    Tet tet;
                    file >> tet.n[0] >> tet.n[1] >> tet.n[2] >> tet.n[3];
                    // convert to 0-based indexing
                    for (int j = 0; j < 4; j++) tet.n[j]--;
                    tets.push_back(tet);
                } else {
                    // skip other element types (triangles, lines etc)
                    std::getline(file, line);
                }
            }
        }
    }

    std::cout << "Loaded " << nodes.size() << " nodes and "
              << tets.size() << " tetrahedra" << std::endl;

    extractSurface();
    computeBounds();
    return true;
}

void Mesh::extractSurface() {
    // a face is on the surface if it only belongs to one tet
    // each tet has 4 faces, each face has 3 nodes
    std::map<std::array<int,3>, int> faceCount;

    auto makeFaceKey = [](int a, int b, int c) {
        std::array<int,3> f = {a, b, c};
        std::sort(f.begin(), f.end());
        return f;
    };

    for (auto& tet : tets) {
        faceCount[makeFaceKey(tet.n[0], tet.n[1], tet.n[2])]++;
        faceCount[makeFaceKey(tet.n[0], tet.n[1], tet.n[3])]++;
        faceCount[makeFaceKey(tet.n[0], tet.n[2], tet.n[3])]++;
        faceCount[makeFaceKey(tet.n[1], tet.n[2], tet.n[3])]++;
    }

    vertexBuffer.clear();
    for (auto& tet : tets) {
        std::array<std::array<int,3>, 4> faces = {{
            makeFaceKey(tet.n[0], tet.n[1], tet.n[2]),
            makeFaceKey(tet.n[0], tet.n[1], tet.n[3]),
            makeFaceKey(tet.n[0], tet.n[2], tet.n[3]),
            makeFaceKey(tet.n[1], tet.n[2], tet.n[3])
        }};

        for (auto& f : faces) {
            if (faceCount[f] == 1) {
                // surface face — add to vertex buffer
                for (int idx : f) {
                    vertexBuffer.push_back(nodes[idx].x);
                    vertexBuffer.push_back(nodes[idx].y);
                    vertexBuffer.push_back(nodes[idx].z);
                }
            }
        }
    }

    std::cout << "Surface faces: " << vertexBuffer.size() / 9 << std::endl;
}

void Mesh::computeBounds() {
    float minX, minY, minZ, maxX, maxY, maxZ;
    minX = minY = minZ = std::numeric_limits<float>::max();
    maxX = maxY = maxZ = std::numeric_limits<float>::lowest();

    for (auto& n : nodes) {
        minX = std::min(minX, n.x); maxX = std::max(maxX, n.x);
        minY = std::min(minY, n.y); maxY = std::max(maxY, n.y);
        minZ = std::min(minZ, n.z); maxZ = std::max(maxZ, n.z);
    }

    center = {(minX + maxX) / 2, (minY + maxY) / 2, (minZ + maxZ) / 2};
    float sizeX = maxX - minX;
    float sizeY = maxY - minY;
    float sizeZ = maxZ - minZ;
    scale = 2.0f / std::max(sizeX, std::max(sizeY, sizeZ));
}