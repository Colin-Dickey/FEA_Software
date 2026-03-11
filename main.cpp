#include "mesh.h"
#include "renderer.h"
#include "solver.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: fea <path_to_msh_file>" << std::endl;
        return -1;
    }

    Mesh mesh;
    if (!mesh.loadMSH(argv[1])) {
        std::cerr << "Failed to load MSH file" << std::endl;
        return -1;
    }

    Solver solver;
    solver.material.E  = 200000;
    solver.material.nu = 0.3;
    solver.solve(mesh);

    Renderer renderer;
    if (!renderer.init(800, 600, "FEA Viewer")) {
        std::cerr << "Failed to init renderer" << std::endl;
        return -1;
    }

    renderer.displacementsPtr = &solver.displacements;
    renderer.loadMesh(mesh, solver.vonMises);
    renderer.renderLoop();
    renderer.cleanup();

    return 0;
}