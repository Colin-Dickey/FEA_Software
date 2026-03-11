#ifndef SOLVER_H
#define SOLVER_H

#include "mesh.h"
#include <Eigen/Sparse>
#include <Eigen/Dense>
#include <vector>

struct Material {
    double E;
    double nu;
};

class Solver {
public:
    Material material;
    std::vector<double> displacements;
    std::vector<double> vonMises;

    void solve(Mesh& mesh);

private:
    Eigen::Matrix<double, 6, 12> computeB(Mesh& mesh, Tet& tet, double& volume);
    Eigen::Matrix<double, 6, 6>  computeD();
    void applyBoundaryConditions(Eigen::SparseMatrix<double>& K,
                                  Eigen::VectorXd& f,
                                  Mesh& mesh);
    void computeStresses(Mesh& mesh);
};

#endif