#include "solver.h"
#include <Eigen/Sparse>
#include <Eigen/Dense>
#include <Eigen/SparseLU>
#include <iostream>
#include <cmath>

// compute the B matrix (strain-displacement) and volume for a tet
Eigen::Matrix<double, 6, 12> Solver::computeB(Mesh& mesh, Tet& tet, double& volume) {
    // get node coordinates
    double x1 = mesh.nodes[tet.n[0]].x, y1 = mesh.nodes[tet.n[0]].y, z1 = mesh.nodes[tet.n[0]].z;
    double x2 = mesh.nodes[tet.n[1]].x, y2 = mesh.nodes[tet.n[1]].y, z2 = mesh.nodes[tet.n[1]].z;
    double x3 = mesh.nodes[tet.n[2]].x, y3 = mesh.nodes[tet.n[2]].y, z3 = mesh.nodes[tet.n[2]].z;
    double x4 = mesh.nodes[tet.n[3]].x, y4 = mesh.nodes[tet.n[3]].y, z4 = mesh.nodes[tet.n[3]].z;

    // jacobian matrix
    Eigen::Matrix4d J;
    J << 1, x1, y1, z1,
         1, x2, y2, z2,
         1, x3, y3, z3,
         1, x4, y4, z4;

    volume = J.determinant() / 6.0;
    if (volume < 0) volume = -volume;

    Eigen::Matrix4d Jinv = J.inverse();

    // shape function derivatives (constant for linear tet)
    double dN1dx = Jinv(1,0), dN1dy = Jinv(2,0), dN1dz = Jinv(3,0);
    double dN2dx = Jinv(1,1), dN2dy = Jinv(2,1), dN2dz = Jinv(3,1);
    double dN3dx = Jinv(1,2), dN3dy = Jinv(2,2), dN3dz = Jinv(3,2);
    double dN4dx = Jinv(1,3), dN4dy = Jinv(2,3), dN4dz = Jinv(3,3);

    // B matrix (6x12)
    Eigen::Matrix<double, 6, 12> B;
    B.setZero();

    B(0,0)=dN1dx;              B(0,3)=dN2dx;              B(0,6)=dN3dx;              B(0,9) =dN4dx;
    B(1,1)=dN1dy;              B(1,4)=dN2dy;              B(1,7)=dN3dy;              B(1,10)=dN4dy;
    B(2,2)=dN1dz;              B(2,5)=dN2dz;              B(2,8)=dN3dz;              B(2,11)=dN4dz;
    B(3,0)=dN1dy; B(3,1)=dN1dx; B(3,3)=dN2dy; B(3,4)=dN2dx; B(3,6)=dN3dy; B(3,7)=dN3dx; B(3,9)=dN4dy;  B(3,10)=dN4dx;
    B(4,1)=dN1dz; B(4,2)=dN1dy; B(4,4)=dN2dz; B(4,5)=dN2dy; B(4,7)=dN3dz; B(4,8)=dN3dy; B(4,10)=dN4dz; B(4,11)=dN4dy;
    B(5,0)=dN1dz; B(5,2)=dN1dx; B(5,3)=dN2dz; B(5,5)=dN2dx; B(5,6)=dN3dz; B(5,8)=dN3dx; B(5,9)=dN4dz;  B(5,11)=dN4dx;

    return B;
}

// material constitutive matrix D (6x6)
Eigen::Matrix<double, 6, 6> Solver::computeD() {
    double E  = material.E;
    double nu = material.nu;
    double c  = E / ((1.0 + nu) * (1.0 - 2.0 * nu));

    Eigen::Matrix<double, 6, 6> D;
    D.setZero();

    D(0,0) = c*(1-nu); D(0,1) = c*nu;    D(0,2) = c*nu;
    D(1,0) = c*nu;    D(1,1) = c*(1-nu); D(1,2) = c*nu;
    D(2,0) = c*nu;    D(2,1) = c*nu;    D(2,2) = c*(1-nu);
    D(3,3) = c*(1-2*nu)/2;
    D(4,4) = c*(1-2*nu)/2;
    D(5,5) = c*(1-2*nu)/2;

    return D;
}

void Solver::solve(Mesh& mesh) {
    int numNodes = mesh.nodes.size();
    int dof      = numNodes * 3;

    std::cout << "Assembling stiffness matrix (" << dof << "x" << dof << ")..." << std::endl;

    Eigen::SparseMatrix<double> K(dof, dof);
    Eigen::VectorXd f = Eigen::VectorXd::Zero(dof);

    std::vector<Eigen::Triplet<double>> triplets;
    Eigen::Matrix<double, 6, 6> D = computeD();

    // assemble global K
    for (auto& tet : mesh.tets) {
        double volume;
        Eigen::Matrix<double, 6, 12> B = computeB(mesh, tet, volume);
        Eigen::Matrix<double, 12, 12> Ke = volume * B.transpose() * D * B;

        // scatter Ke into global K
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                for (int di = 0; di < 3; di++) {
                    for (int dj = 0; dj < 3; dj++) {
                        int row = tet.n[i] * 3 + di;
                        int col = tet.n[j] * 3 + dj;
                        triplets.push_back({row, col, Ke(i*3+di, j*3+dj)});
                    }
                }
            }
        }
    }

    K.setFromTriplets(triplets.begin(), triplets.end());
    std::cout << "Stiffness matrix assembled." << std::endl;

    // apply boundary conditions
    applyBoundaryConditions(K, f, mesh);

    // solve K*u = f
    std::cout << "Solving..." << std::endl;
    Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
    solver.analyzePattern(K);
    solver.factorize(K);
    Eigen::VectorXd u = solver.solve(f);

    displacements.resize(dof);
    for (int i = 0; i < dof; i++) displacements[i] = u[i];

    std::cout << "Solved! Max displacement: "
              << u.cwiseAbs().maxCoeff() << "mm" << std::endl;

    computeStresses(mesh);
}

void Solver::applyBoundaryConditions(Eigen::SparseMatrix<double>& K,
                                      Eigen::VectorXd& f,
                                      Mesh& mesh) {
    int numNodes = mesh.nodes.size();
    double zMin  =  1e10;
    double zMax  = -1e10;

    for (auto& n : mesh.nodes) {
        zMin = std::min(zMin, (double)n.z);
        zMax = std::max(zMax, (double)n.z);
    }

    // compute cross section area for pressure calculation
    // area = 10mm x 10mm = 100 mm²
    // pressure = F / A = 1000 / 100 = 10 MPa
    double tol = 1e-6;

    // first pass - fix bottom nodes
    int fixedCount = 0;
    for (int i = 0; i < numNodes; i++) {
        if (std::abs(mesh.nodes[i].z - zMin) < tol) {
            for (int d = 0; d < 3; d++) {
                K.coeffRef(i * 3 + d, i * 3 + d) += 1e30;
            }
            fixedCount++;
        }
    }
    std::cout << "Fixed nodes: " << fixedCount << std::endl;

    // find surface triangles on top face and integrate pressure over them
    // build face->tet map
    std::map<std::array<int,3>, int> faceCount;
    auto makeFaceKey = [](int a, int b, int c) {
        std::array<int,3> f = {a, b, c};
        std::sort(f.begin(), f.end());
        return f;
    };

    // store original face (with winding) for each sorted key
    std::map<std::array<int,3>, std::array<int,3>> faceOrig;

    for (auto& tet : mesh.tets) {
        std::array<std::array<int,3>, 4> faces = {{
            {tet.n[0], tet.n[1], tet.n[2]},
            {tet.n[0], tet.n[1], tet.n[3]},
            {tet.n[0], tet.n[2], tet.n[3]},
            {tet.n[1], tet.n[2], tet.n[3]}
        }};
        for (auto& face : faces) {
            auto key = makeFaceKey(face[0], face[1], face[2]);
            faceCount[key]++;
            faceOrig[key] = face;
        }
    }

    // pressure in z direction (compressive)
    double pressure = 1000.0 / 100.0; // 10 MPa = 10 N/mm²
    int triCount = 0;
    double totalForce = 0.0;

    for (auto& [key, count] : faceCount) {
        if (count != 1) continue; // skip internal faces

        auto& face = faceOrig[key];
        int n0 = face[0], n1 = face[1], n2 = face[2];

        // check if all 3 nodes are on top face
        if (std::abs(mesh.nodes[n0].z - zMax) > tol) continue;
        if (std::abs(mesh.nodes[n1].z - zMax) > tol) continue;
        if (std::abs(mesh.nodes[n2].z - zMax) > tol) continue;

        // compute triangle area
        Eigen::Vector3d p0(mesh.nodes[n0].x, mesh.nodes[n0].y, mesh.nodes[n0].z);
        Eigen::Vector3d p1(mesh.nodes[n1].x, mesh.nodes[n1].y, mesh.nodes[n1].z);
        Eigen::Vector3d p2(mesh.nodes[n2].x, mesh.nodes[n2].y, mesh.nodes[n2].z);

        double area = 0.5 * (p1 - p0).cross(p2 - p0).norm();

        // distribute pressure force equally to 3 nodes of triangle
        // f = pressure * area / 3 per node in z direction
        double nodalForce = pressure * area / 3.0;

        f[n0 * 3 + 2] += nodalForce;
        f[n1 * 3 + 2] += nodalForce;
        f[n2 * 3 + 2] += nodalForce;

        totalForce += pressure * area;
        triCount++;
    }

    std::cout << "Pressure triangles: " << triCount << std::endl;
    std::cout << "Total force applied: " << totalForce << " N" << std::endl;
}

void Solver::computeStresses(Mesh& mesh) {
    Eigen::Matrix<double, 6, 6> D = computeD();
    int numNodes = mesh.nodes.size();

    std::vector<double> stressSum(numNodes, 0.0);
    std::vector<int>    stressCount(numNodes, 0);

    for (auto& tet : mesh.tets) {
        double volume;
        Eigen::Matrix<double, 6, 12> B = computeB(mesh, tet, volume);

        // get element displacements
        Eigen::Matrix<double, 12, 1> ue;
        for (int i = 0; i < 4; i++) {
            ue[i*3+0] = displacements[tet.n[i]*3+0];
            ue[i*3+1] = displacements[tet.n[i]*3+1];
            ue[i*3+2] = displacements[tet.n[i]*3+2];
        }

        // stress = D * B * ue
        Eigen::Matrix<double, 6, 1> stress = D * B * ue;

        // von mises stress
        double sx = stress[0], sy = stress[1], sz = stress[2];
        double txy = stress[3], tyz = stress[4], txz = stress[5];
        double vm = std::sqrt(0.5 * ((sx-sy)*(sx-sy) + (sy-sz)*(sy-sz) +
                    (sz-sx)*(sz-sx) + 6*(txy*txy + tyz*tyz + txz*txz)));

        // average to nodes
        for (int i = 0; i < 4; i++) {
            stressSum[tet.n[i]]   += vm;
            stressCount[tet.n[i]] += 1;
        }
    }

    vonMises.resize(numNodes);
    for (int i = 0; i < numNodes; i++) {
        vonMises[i] = stressCount[i] > 0 ? stressSum[i] / stressCount[i] : 0.0;
    }

    double maxVM = *std::max_element(vonMises.begin(), vonMises.end());
    std::cout << "Max Von Mises stress: " << maxVM << "MPa" << std::endl;
}
