// Gmsh project created on Wed Mar 11 15:55:49 2026

lc = 2;

Point(1) = {0, 0, 0, lc};
Point(2) = {10, 0, 0, lc};
Point(3) = {10, 10, 0, lc};
Point(4) = {0, 10, 0, lc};

Line(1) = {1, 2};
Line(2) = {3, 2};
Line(3) = {3, 4};
Line(4) = {4, 1};

Curve Loop(1) = {4, 1, -2, 3};

Plane Surface(1) = {1};

Extrude {0, 0, 5 * 10} { Surface{1}; }

Mesh 3;

Mesh.MshFileVersion = 2.2;
Save "cuboid.msh";