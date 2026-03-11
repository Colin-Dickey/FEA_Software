#include "mesh.h"
#include "renderer.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: fea <path_to_stl_file>" << std::endl;
        return -1;
    }

    Mesh mesh;
    if (!mesh.loadSTL(argv[1])) {
        std::cerr << "Failed to load STL file" << std::endl;
        return -1;
    }

    Renderer renderer;
    if (!renderer.init(800, 600, "FEA Viewer")) {
        std::cerr << "Failed to init renderer" << std::endl;
        return -1;
    }

    renderer.loadMesh(mesh);
    renderer.renderLoop();
    renderer.cleanup();

    return 0;
}