// CFloatMatrix.h: interface for the CFloatMatrix class.
//
//////////////////////////////////////////////////////////////////////

#ifndef AFX_CFLOATMATRIX_H__9DC25951_78AB_11D2_91ED_00104BDB3E63__INCLUDED_
#define AFX_CFLOATMATRIX_H__9DC25951_78AB_11D2_91ED_00104BDB3E63__INCLUDED_

#include <string.h>
#include <math.h>
 
struct Complex { double r,i; };

inline float sqr(float x) { return x*x; }

/// Manages an N*M array of floats. Horribly wasteful on memory I guess but makes for easy plug-and-play.
class CFloatMatrix
{
	public:
		/// default constructor
		CFloatMatrix();
		/// initializing constructor
		CFloatMatrix(int rows,int columns);
		/// destructor
		~CFloatMatrix();
		/// (re-)initialization sets the size of the array
		void init(int rows,int columns);

		/// copy constructor
		CFloatMatrix(const CFloatMatrix& m);
		/// assignment operator
		CFloatMatrix operator=(const CFloatMatrix& m);

		/// returns whether the matrix has allocated its memory
		bool IsInitialized() { return m_data!=NULL; }

		/// safe assignment access to an entry in the array
		void set(int r,int c,float value);
		/// safe retrieval access to an entry in the array
		float get(int r,int c) const;

		/// returns the number of rows in the matrix
		int GetNRows() const { return m_nrows; }
		/// returns the number of columns in the matrix 
		int GetNColumns() const { return m_ncolumns; }
		/// returns a CSize structure containing the size (.cx,.cy)
		//CSize Size() const { return CSize(m_nrows,m_ncolumns); }  // 200531 rh: not used in classificationTools.cpp
		/// checks if the matrix m is the same size as this one
		bool IsSameSizeAs(const CFloatMatrix& m) const
			{ return((m_nrows==m.m_nrows)&&(m_ncolumns==m.m_ncolumns)); }
		/// fills the matrix with zeros
		void MakeZero() { for(int i=0;i<m_nrows*m_ncolumns;i++) m_data[i]=0.0F; }
		/// files the matrix with zeros other than ones down the diagonal
		void MakeIdentity();
		/// inverts the matrix
		void Invert() { *this = Inverse(*this); }

		/// extracts a single column of the matrix into a new (n_rows x 1) matrix
		CFloatMatrix GetColumn(int c) const;
		/// copies the (n_rows x 1) matrix into the specified column
		void SetColumn(int c,CFloatMatrix col);

		/// extracts a single row of the matrix into a new (1 x n_columns) matrix
		CFloatMatrix GetRow(int r) const;
		/// copies the (1 x n_columns) matrix into the specified row
		void SetRow(int c,CFloatMatrix row);

		/// extracts the first cols columns into a new (n_rows x cols) matrix
		CFloatMatrix GetColumns(int cols) const;
		/// extracts the first rows rows into a new (rows x n_cols) matrix
		CFloatMatrix GetRows(int rows) const;

		/// applies Principal Components Analysis to this data matrix
		void PCA(CFloatMatrix& eigenvectors, CFloatMatrix& eigenvalues) const;

		/// performs eigen-decomposition (matrix is symmetric)
		void PCAonCovar(CFloatMatrix &eigenvectors,CFloatMatrix &eigenvalues) const;

		/// returns true if the matrix is symmetric
		bool IsSymmetric() const;

		/// returns the sum of the entries in the matrix
		float Sum() { float t=0.0F; for(int i=0;i<m_nrows*m_ncolumns;i++) t+=m_data[i]; return t;}

		/// returns a point to the data - use sparingly
		float* GetDataPtr() { return m_data; }

		CFloatMatrix operator+=(const CFloatMatrix& m);
		CFloatMatrix operator-=(const CFloatMatrix& m);
		CFloatMatrix operator*=(float f);
		CFloatMatrix ScalarMultiply(const CFloatMatrix& m);
		CFloatMatrix operator/=(float f);
		bool operator==(const CFloatMatrix& v) const;
		bool operator!=(const CFloatMatrix& v) const;
		float SumOfSquares();
		float SquareRootOfSumOfSquares() { return (float)sqrt(SumOfSquares()); }
		friend float SumOfSquares(CFloatMatrix a) { return a.SumOfSquares(); }
		friend float SquareRootOfSumOfSquares(CFloatMatrix a) { return a.SquareRootOfSumOfSquares(); }
		friend CFloatMatrix SqrEntries(CFloatMatrix a);

		friend CFloatMatrix Transpose(CFloatMatrix a);
		friend CFloatMatrix Normalize(CFloatMatrix v);
		friend CFloatMatrix Inverse(CFloatMatrix a);

		// binary operators
		friend CFloatMatrix operator*(CFloatMatrix a,float f);
		friend CFloatMatrix operator*(float f,CFloatMatrix a);
		friend CFloatMatrix operator*(CFloatMatrix a,CFloatMatrix b);
		friend CFloatMatrix operator/(CFloatMatrix a,float f);
		friend CFloatMatrix operator/(float f,CFloatMatrix a);
		friend CFloatMatrix operator+(CFloatMatrix a,CFloatMatrix b);
		friend CFloatMatrix operator-(CFloatMatrix a,CFloatMatrix b);
		friend float DotProduct(CFloatMatrix a,CFloatMatrix b);

		void ConvertQuaternion(CFloatMatrix q);

	protected:

		int m_nrows,m_ncolumns;
		/// the data
		float *m_data;

	protected:

		/// swaps the two specified rows
		void SwapRows(int i,int j);
};

#endif // AFX_CFLOATMATRIX_H__9DC25951_78AB_11D2_91ED_00104BDB3E63__INCLUDED_
