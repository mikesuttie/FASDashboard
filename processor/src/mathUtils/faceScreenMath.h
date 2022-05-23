#ifndef FACESCREENMATH_H
#define FACESCREENMATH_H

#include <cmath> // for sqrt (double)
#include <iostream>

namespace faceScreenMath
{
	const float CHI_SQUARE_99[100] = { 6.635,9.21,11.345,13.277,15.086,16.812,18.475,20.09,21.666,23.209,
	24.725,26.217,27.688,29.141,30.578,32,33.409,34.805,36.191,37.566,38.932,40.289,41.638,42.98,44.314,
	45.642,46.963,48.278,49.588,50.892,52.191,53.486,54.776,56.061,57.342,58.619,59.893,61.162,62.428,
	63.691,64.95,66.206,67.459,68.71,69.957,71.201,72.443,73.683,74.919,76.154,77.386,78.616,79.843,
	81.069,82.292,83.513,84.733,85.95,87.166,88.379,89.591,90.802,92.01,93.217,94.422,95.626,96.828,
	98.028,99.228,100.425,101.621,102.816,104.01,105.202,106.393,107.583,108.771,109.958,111.144,112.329,
	113.512,114.695,115.876,117.057,118.236,119.414,120.591,121.767,122.942,124.116,125.289,126.462,
	127.633,128.803,129.973,131.141,132.309,133.476,134.642,135.807 };

	const float CHI_SQUARE_97PT5[100] = { 5.024,7.378,9.348,11.143,12.833,14.449,16.013,17.535,19.023,20.483,21.92,23.337,24.736,26.119,27.488,28.845,30.191,
	31.526,32.852,34.17,35.479,36.781,38.076,39.364,40.646,41.923,43.195,44.461,45.722,46.979,48.232,49.48,50.725,51.966,
	53.203,54.437,55.668,56.896,58.12,59.342,60.561,61.777,62.99,64.201,65.41,66.617,67.821,69.023,70.222,71.42,72.616,
	73.81,75.002,76.192,77.38,78.567,79.752,80.936,82.117,83.298,84.476,85.654,86.83,88.004,89.177,90.349,91.519,92.689,
	93.856,95.023,96.189,97.353,98.516,99.678,100.839,101.999,103.158,104.316,105.473,106.629,107.783,108.937,110.09,111.242,
	112.393,113.544,114.693,115.841,116.989,118.136,119.282,120.427,121.571,122.715,123.858,125,126.141,127.282,128.422,129.561 };

	const float CHI_SQUARE_90[100] = { 2.706,4.605,6.251,7.779,9.236,10.645,12.017,13.362,14.684,
	15.987,17.275,18.549,19.812,21.064,22.307,23.542,24.769,25.989,27.204,28.412,29.615,30.813,
	32.007,33.196,34.382,35.563,36.741,37.916,39.087,40.256,41.422,42.585,43.745,44.903,46.059,
	47.212,48.363,49.513,50.66,51.805,52.949,54.09,55.23,56.369,57.505,58.641,59.774,60.907,
	62.038,63.167,64.295,65.422,66.548,67.673,68.796,69.919,71.04,72.16,73.279,74.397,75.514,
	76.63,77.745,78.86,79.973,81.085,82.197,83.308,84.418,85.527,86.635,87.743,88.85,89.956,
	91.061,92.166,93.27,94.374,95.476,96.578,97.68,98.78,99.88,100.98,102.079,103.177,104.275,
	105.372,106.469,107.565,108.661,109.756,110.85,111.944,113.038,114.131,115.223,116.315,
	117.407,118.498 };


	//------------------------------------------------------------------------
	// Matrix ops. Some taken from vtkThinPlateSplineTransform.cxx
	static inline double** NewMatrix(int rows, int cols)
	{
		double* matrix = new double[rows * cols];

		// allocation could have failed here
		if (matrix == nullptr)
		{
			std::cerr << "Memory allocation failed in NewMatrix in vtkSurfacePCA!" << std::endl;
			// can we do anything else?
		}

		double** m = new double* [rows];

		// allocation could have failed here
		if (m == nullptr)
		{
			std::cerr << "Memory allocation failed in NewMatrix in vtkSurfacePCA!" << std::endl;
			// can we do anything else?
		}

		for (int i = 0; i < rows; i++) {
			m[i] = &matrix[i * cols];
		}
		return m;
	}

	//------------------------------------------------------------------------
	static inline void DeleteMatrix(double** m)
	{
		delete[] * m;
		delete[] m;
	}

	//------------------------------------------------------------------------
	static inline void MatrixMultiply(double** a, double** b, double** c,
		int arows, int acols,
		int brows, int bcols)
	{
		if (acols != brows) {
			return; // acols must equal br otherwise we can't proceed
		}

		// c must have size arows*bcols (we assume this)

		for (int i = 0; i < arows; i++) {
			for (int j = 0; j < bcols; j++) {
				c[i][j] = 0.0;
				for (int k = 0; k < acols; k++) {
					c[i][j] += a[i][k] * b[k][j];
				}
			}
		}
	}

	//------------------------------------------------------------------------
	// Subtracting the mean column from the observation matrix is equal
	// to subtracting the mean shape from all shapes.
	// The mean column is equal to the Procrustes mean (it is also returned)
	static inline void SubtractMeanColumn(double** m, double* mean, int rows, int cols)
	{
		int r, c;
		double csum;
		for (r = 0; r < rows; r++) {
			csum = 0.0F;
			for (c = 0; c < cols; c++) {
				csum += m[r][c];
			}
			// calculate average value of row
			csum /= cols;

			// Mean shape vector is updated
			mean[r] = csum;

			// average value is subtracted from all elements in the row
			for (c = 0; c < cols; c++) {
				m[r][c] -= csum;
			}
		}
	}

	//------------------------------------------------------------------------
	// Normalise all columns to have length 1
	// meaning that all eigenvectors are normalised
	static inline void NormaliseColumns(double** m, int rows, int cols)
	{
		for (int c = 0; c < cols; c++) {
			double cl = 0;
			for (int r = 0; r < rows; r++) {
				cl += m[r][c] * m[r][c];
			}
			cl = std::sqrt(cl);

			// If cl == 0 something is rotten, dont do anything now
			if (cl != 0) {
				for (int r = 0; r < rows; r++) {
					double sd = m[r][c];
					m[r][c] /= cl;
					double ds = m[r][c];

				}
			}
		}
	}

	//------------------------------------------------------------------------
	// Here it is assumed that a rows >> a cols
	// Output matrix is [a cols X a cols]
	static inline void SmallCovarianceMatrix(double** a, double** c,
		int arows, int acols)
	{
		const int s = acols;

		// c must have size acols*acols (we assume this)
		for (int i = 0; i < acols; i++) {
			for (int j = 0; j < acols; j++) {
				// Use symmetry
				if (i <= j) {
					c[i][j] = 0.0;
					for (int k = 0; k < arows; k++) {
						c[i][j] += a[k][i] * a[k][j];
					}
					c[i][j] /= (s - 1);
					c[j][i] = c[i][j];
				}
			}
		}
	}

	//------------------------------------------------------------------------
	static inline double* NewVector(int length)
	{
		double* vec = new double[length];

		// allocation could have failed here
		if (vec == nullptr)
		{
			std::cerr << "Memory allocation failed in NewVector in vtkSurfacePCA!" << std::endl;
			// can we do anything else?
		}

		return vec;
	}

	//------------------------------------------------------------------------
	static inline void DeleteVector(double* v)
	{
		delete[] v;
	}

} //namespace faceScreenMath
#endif // FACESCREENMATH_H