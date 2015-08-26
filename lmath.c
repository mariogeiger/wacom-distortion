#include "lmath.h"
#include <stdlib.h>
#include <math.h>

/* P A = L U
 * P : permutation matrix
 * L : lower matrix
 * U : upper matrix
*/
static void
lu_decomposition(int n, const double* A, double* P, double* L, double* U)
{
	int k,i,j,r;
	double d, e;
	r = 0; // To avoid warning: 'r' may be used uninitialized in this function

	// P,L = identity
	for (i = 0; i < n*n; ++i) {
		P[i] = 0.0;
		L[i] = 0.0;
	}
	for (i = 0; i < n*n; i += n+1) {
		P[i] = 1.0;
		L[i] = 1.0;
	}

	// U = A
	for (i = 0; i < n*n; ++i) U[i] = A[i];

	for (k = 0; k < n; ++k) {
		// find maximum in column k
		d = -1.0;
		for (i = k; i < n; ++i) {
			e = fabs(U[i*n+k]);
			if (e > d) {
				r = i;
				d = e;
			}
		}

		if (d > 0.0) {
			// swap lines r,k
			if (r != k) {
				for (i = 0; i < n; ++i) {
					d = U[r*n+i];
					U[r*n+i] = U[k*n+i];
					U[k*n+i] = d;

					d = P[r*n+i];
					P[r*n+i] = P[k*n+i];
					P[k*n+i] = d;
				}
				for (i = 0; i < k; ++i) {
					d = L[r*n+i];
					L[r*n+i] = L[k*n+i];
					L[k*n+i] = d;
				}
			}

			// make 0's
			for (i = k+1; i < n; ++i) {
				d = L[i*n+k] = U[i*n+k] / U[k*n+k];
				for (j = 0; j < n; ++j)
					U[i*n+j] -= d * U[k*n+j];
			}
		}
	}
}

/* LUx = b */
static int
solve_lu(int n, const double* L, const double* U, const double* b, double* x)
{
	int k, i;
	int res = 0;
	double s;
	double* y = (double*)malloc(sizeof(double)*n);

	for (i = 0; i < n; ++i) y[i] = 0.0;

	// Ly = b
	for (k = 0; k < n; ++k) {
		s = 0.0;
		for (i = 0; i < n; ++i) s += L[k*n+i] * y[i];
		y[k] = b[k] - s;
	}

	for (i = 0; i < n; ++i) x[i] = 0.0;
	// Ux = y
	for (k = n-1; k >= 0; --k) {
		if (U[k*n+k] == 0.0) {
			res = 1;
			goto finish;
		}
		s = 0.0;
		for (i = 0; i < n; ++i) s += U[k*n+i] * x[i];
		x[k] = (y[k] - s) / U[k*n+k];
	}
finish:
	free(y);
	return res;
}


/* Ax = b */
int solve_ls(int n, const double* A, const double* b, double* x)
{
	int i, k;
	double *P,*L,*U,*Pb;
	P  = (double*)malloc(sizeof(double)*n*n);
	L  = (double*)malloc(sizeof(double)*n*n);
	U  = (double*)malloc(sizeof(double)*n*n);
	Pb = (double*)malloc(sizeof(double)*n);

	lu_decomposition(n, A, P, L, U);

	for (i = 0; i < n; ++i) {
		Pb[i] = 0;
		for (k = 0; k < n; ++k) {
			Pb[i] += P[i*n+k] * b[k];
		}
	}
	i = solve_lu(n, L, U, Pb, x);

	free(P);
	free(L);
	free(U);
	free(Pb);
	return i;
}

/* Solve min ||Ax - b|| where A is dimention nxm */
int least_squares(int n, int m, const double* A, const double* b, double* x)
{
	// m should be smaller than n

	int i, j, k;
	double d;
	double *ATA = (double*)malloc(sizeof(double)*m*m);
	double *ATb = (double*)malloc(sizeof(double)*m);

	// compute A^t A
	for (i = 0; i < m; ++i) {
		for (j = 0; j < m; ++j) {
			d = 0.0;
			for (k = 0; k < n; ++k) {
				d += A[k*m+i] * A[k*m+j];
			}
			ATA[i*m+j] = d;
		}
	}

	// compute A^t b
	for (i = 0; i < m; ++i) {
		d = 0.0;
		for (k = 0; k < n; ++k) {
			d += A[k*m+i] * b[k];
		}
		ATb[i] = d;
	}

	i = solve_ls(m, ATA, ATb, x);
	free(ATA);
	free(ATb);
	return i;
}

/* minimize || Ax - b || in x
 * under constraint Cx = e
 *
 * A : n x m Matrix
 * b : n Vector
 * C : p x m Matrix
 * e : p Vector
 *
 * m = number of variables
 * n = number of equations to minimize
 * p = number of equations to respect
 */
int least_squares_constraint(int m, int n, int p,
								  const double* A, const double* b,
								  const double* C, const double* e,
								  double* x)
{
	/* Solve the following system :
	 * [2A^t A   -C^t] [x]   [2A^t b]
	 * [             ] [ ] = [      ]
	 * [  C       0  ] [L]   [  e   ]
	 * Where L is a vector of Lagrange multiplier
	 */

	int i, j, k;
	int u = p + m;
	double tmp;
	double* matrix = (double*)malloc(sizeof(double)*u*u);
	double* sol   = (double*)malloc(sizeof(double)*u);
	double* rhs   = (double*)malloc(sizeof(double)*u);

	// Write in matrix
	//for (i = 0; i < u*u; ++i) matrix[i] = 0.0;

	for (i = 0; i < m; ++i) for (j = 0; j < m; ++j) {
		tmp = 0.0;
		for (k = 0; k < n; ++k) tmp += A[k*m+i] * A[k*m+j];
		matrix[  i  *u+  j] = 2.0 * tmp;
	}
	for (i = 0; i < m; ++i) for (j = 0; j < p; ++j)
		matrix[  i  *u+m+j] = -C[j*m+i];

	for (i = 0; i < p; ++i) for (j = 0; j < m; ++j)
		matrix[(m+i)*u+  j] =  C[i*m+j];

	for (i = 0; i < p; ++i) for (j = 0; j < p; ++j)
		matrix[(m+i)*u+m+j] =  0.0;

	// Write in newb
	for (i = 0; i < m; ++i) {
		tmp = 0.0;
		for (k = 0; k < n; ++k) tmp += A[k*m+i] * b[k];
		rhs[i] = 2 * tmp;
	}
	for (i = 0; i < p; ++i)
		rhs[m+i] = e[i];

	// Solve the system
	k = solve_ls(u, matrix, rhs, sol);

	for (i = 0; i < m; ++i) x[i] = sol[i];

	free(matrix);
	free(rhs);
	free(sol);
	return k;
}
