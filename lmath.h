#ifndef LMATH_H
#define LMATH_H

/* Solve Ax = b where A is square with an LU factorization */
int solve_ls(int n, const double* A, const double* b, double* x);




/* Solve min ||Ax - b|| where A is dimention nxm */
int least_squares(int n, int m, const double* A, const double* b, double* x);






/* minimize || Ax - b || in x
 * under constraint Cx = e
 *
 * A : n x m Matrix
 * b : n Vector
 * C : p x m Matrix
 * e : p Vector
 */
int least_squares_constraint(int m, int n, int p,
								  const double* A, const double* b,
								  const double* C, const double* e,
								  double* x);


#endif // LMATH_H
