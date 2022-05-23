// CFloatMatrix.cpp: implementation of the CFloatMatrix class.
//
//////////////////////////////////////////////////////////////////////

#include "../mathUtils/C2dVector.h"
#include "CFloatMatrix.h"

#include <string.h>
#include <math.h>
#include <float.h>
#include <vtkMath.h>

//#include "tjhGraphWindow.h"

#define ASSERT(x) // TODO: remove this deactivating macro

/*
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif
*/

void tred2(float **a, int n, float d[], float e[]);
void tqli(float d[], float e[], int n, float **z);
void eigsrt(float d[], float **v, int n);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//IMPLEMENT_SERIAL( CFloatMatrix, CObject, 1 )

CFloatMatrix::CFloatMatrix()
{
	m_data=NULL;
	m_nrows=m_ncolumns=0;
}

void CFloatMatrix::init(int rows,int columns)
{
	// if is already the right size then we don't need to do anything
	if(rows*columns==m_nrows*m_ncolumns) { m_nrows=rows; m_ncolumns=columns; return; }

	// may have been previously used, deallocate memory if so
	if(m_data!=NULL) delete []m_data;

	// initialize as usual
	m_data = new float[rows*columns];
	if(m_data)
	{
		m_nrows = rows;
		m_ncolumns = columns;
	}
	else
	{
		// memory allocation did not succeed!
		ASSERT(false);
		m_data=NULL;
		m_nrows=m_ncolumns=0;
	}
}

CFloatMatrix::CFloatMatrix(int rows,int columns)
{
	m_data = new float[rows*columns];
	if(m_data)
	{
		m_nrows = rows;
		m_ncolumns = columns;
	}
	else
	{
		// memory allocation did not succeed!
		ASSERT(false);
		m_data=NULL;
		m_nrows=m_ncolumns=0;
	}
}

CFloatMatrix::~CFloatMatrix()
{
	if(m_data!=NULL) delete []m_data;
}

CFloatMatrix::CFloatMatrix(const CFloatMatrix& m)
{
	m_data = new float[m.GetNRows()*m.GetNColumns()];
	if(m_data)
	{
		m_nrows=m.GetNRows();
		m_ncolumns=m.GetNColumns();
		int i;
		for(i=0;i<m_nrows*m_ncolumns;i++)
			m_data[i]=m.m_data[i];
	}
	else
	{
		// memory allocation did not succeed!
		ASSERT(false);
		m_data=NULL;
		m_nrows=m_ncolumns=0;
	}
}

CFloatMatrix CFloatMatrix::operator=(const CFloatMatrix& m)
{
    // resize the data array to match
	if(m_data!=NULL) delete []m_data;
	m_data = new float[m.GetNRows()*m.GetNColumns()];
    m_nrows = m.GetNRows();
    m_ncolumns = m.GetNColumns();

    // copy across the data
	int i;
	for(i=0;i<m_nrows*m_ncolumns;i++)
		m_data[i]=m.m_data[i];

	return *this;
}

void CFloatMatrix::set(int r,int c,float value)
{
	ASSERT(m_data);
	ASSERT(r>=0);
	ASSERT(r<m_nrows);
	ASSERT(c>=0);
	ASSERT(c<m_ncolumns);
	m_data[r*m_ncolumns + c]=value;
}

float CFloatMatrix::get(int r,int c) const
{
	ASSERT(m_data);
	ASSERT(r>=0);
	ASSERT(r<m_nrows);
	ASSERT(c>=0);
	ASSERT(c<m_ncolumns);
	return m_data[r*m_ncolumns + c];
}

CFloatMatrix CFloatMatrix::GetColumn(int c) const
{
	CFloatMatrix column(m_nrows,1);
	for(int r=0;r<m_nrows;r++)
		column.set(r,0,get(r,c));
	return column;
}

CFloatMatrix CFloatMatrix::GetColumns(int cols) const
{
	ASSERT(m_data);

	CFloatMatrix P(m_nrows,cols);
	for(int i=0;i<cols;i++)
		P.SetColumn(i,GetColumn(i));
	return P;
}

CFloatMatrix CFloatMatrix::GetRow(int r) const
{
	CFloatMatrix row(1,m_ncolumns);
	for(int c=0;c<m_ncolumns;c++)
		row.set(0,c,get(r,c));
	return row;
}

CFloatMatrix CFloatMatrix::GetRows(int rows) const
{
	ASSERT(m_data);

	CFloatMatrix P(rows,m_ncolumns);
	for(int i=0;i<rows;i++)
		P.SetRow(i,GetRow(i));
	return P;
}

void CFloatMatrix::SetColumn(int c,CFloatMatrix col)
{
	ASSERT(col.m_nrows==m_nrows);
	for(int r=0;r<m_nrows;r++)
		set(r,c,col.get(r,0));
}

void CFloatMatrix::SetRow(int r,CFloatMatrix row)
{
	ASSERT(row.m_ncolumns==m_ncolumns);
	for(int c=0;c<m_ncolumns;c++)
		set(r,c,row.get(0,c));
}

CFloatMatrix CFloatMatrix::operator+=(const CFloatMatrix& m)
{
	// matrices must be of same dimensions
	if(IsSameSizeAs(m))
	{
		// add the contents of m to this matrix
		for(int i=0;i<m_nrows*m_ncolumns;i++)
			m_data[i]+=m.m_data[i];
	}
	else
	{
		// sizes did not match
		ASSERT(false);
	}
	return *this;
}

CFloatMatrix CFloatMatrix::operator-=(const CFloatMatrix& m)
{
	// matrices must be of same dimensions
	if(IsSameSizeAs(m))
	{
		// subtract the contents of m from this matrix
		for(int i=0;i<m_nrows*m_ncolumns;i++)
			m_data[i]-=m.m_data[i];
	}
	else
	{
		// sizes did not match
		ASSERT(false);
	}
	return *this;
}

CFloatMatrix CFloatMatrix::operator*=(float f)
{
	ASSERT(m_data);
	// multiply each entry by f
	for(int i=0;i<m_nrows*m_ncolumns;i++)
		m_data[i]*=f;
	return *this;
}

CFloatMatrix CFloatMatrix::ScalarMultiply(const CFloatMatrix& m)
{
	// matrices must be the same size
	ASSERT(m_data);
	ASSERT(m_nrows==m.m_nrows);
	ASSERT(m_ncolumns==m.m_ncolumns);
	for(int i=0;i<m_nrows*m_ncolumns;i++)
		m_data[i]*=m.m_data[i];
	return *this;
}

CFloatMatrix CFloatMatrix::operator/=(float f)
{
	ASSERT(m_data);
	// divide each entry by f
	for(int i=0;i<m_nrows*m_ncolumns;i++)
		m_data[i]/=f;
	return *this;
}

bool CFloatMatrix::operator==(const CFloatMatrix& m) const
{
	if(!IsSameSizeAs(m)) return false;
	for(int i=0;i<m_nrows*m_ncolumns;i++)
		if(m_data[i]!=m.m_data[i]) return false;
	return true;
}

bool CFloatMatrix::operator!=(const CFloatMatrix& m) const
{
	return !(*this==m);
}

void CFloatMatrix::MakeIdentity()
{
	// can only work on square matrices
	if(m_nrows==m_ncolumns)
	{
		MakeZero();
		for(int i=0;i<m_nrows;i++)
			set(i,i,1.0F);
	}
}

void CFloatMatrix::SwapRows(int i,int j)
{
	CFloatMatrix temp(GetRow(i));
	SetRow(i,GetRow(j));
	SetRow(j,temp);
}

void CFloatMatrix::PCA(CFloatMatrix& eigenvectors, CFloatMatrix& eigenvalues) const
{
	//new tjhImageWindow(title+" input matrix",*this);

	// run PCA on this matrix, each column is a vector
	// compute covariance matrix of either the rows or the columns, whichever gives
	// the smaller result (for speed). If we do it on the columns then we need to convert 
    // the eigenvectors afterwards.

	const int n = GetNRows();    // number of elements in each example
	const int s = GetNColumns(); // sample size

	// compute the mean of the samples
	CFloatMatrix mean(n,1);
	mean.MakeZero();
	for(int i=0;i<s;i++)
		mean += GetColumn(i);
	mean /= (float)s;

	//decide which eigen-decomposition we need to do
	if(s<n)
	{
		//AfxMessageBox("s is smaller than n, running transposed PCA");
		// since s is smaller than n, is best to find eigenthings for s*s matrix
		CFloatMatrix D(n,s),dx(n,1);
		for(int i=0;i<s;i++)
		{
			// get the difference between this shape and the mean
			dx = GetColumn(i)-mean;
			// add it into D as a separate column
			D.SetColumn(i,dx);
		}

		// then compute the matrix T from D
		CFloatMatrix T(s,s);
		T = Transpose(D)*D;
		T /= (float)s;

		// check that the matrix is symmetric
		if(!T.IsSymmetric())
		{
			std::cerr << "In CFloatMatrix.cpp::PCA :  Internal error, covariance matrix T is not symmetric!" << std::endl;
			ASSERT(false);
			exit(-1);
		}

		//new tjhImageWindow(title + " covariance matrix",T);

		// compute the eigenthings of T
		CFloatMatrix other_eigenvalues;
		CFloatMatrix other_eigenvectors;
		T.PCAonCovar(other_eigenvectors,other_eigenvalues);

		// work out the eigenvectors we want
		CFloatMatrix desired_vectors(n,s);
		desired_vectors = D * other_eigenvectors;
		eigenvectors.MakeZero();
		eigenvalues.MakeZero();
		for(int i=0;i<s;i++) 
		{
			eigenvectors.SetColumn(i,Normalize(desired_vectors.GetColumn(i)));
			eigenvalues.set(i,0,other_eigenvalues.get(i,0));
			// (leave the other entries as zero)
		}
	}
	else
	{
		//AfxMessageBox("s is larger than or equal to n, running straight PCA");
		// since s is larger than n, is best to find eigenthings for n*n matrix
		CFloatMatrix dx(n,1),S(n,n);

		// -- build the covariance matrix
		S.MakeZero();
		C2dVector p;



		for(int i=0;i<s;i++)
		{
			// get the difference between this shape and the mean
			dx = GetColumn(i)-mean;
			// add it into the covariance matrix
			S+=dx*Transpose(dx);
		}
		S /= (float)s;

		// check that the matrix is symmetric
		if(!S.IsSymmetric())
		{
			std::cerr << "In CFloatMatrix.cpp::PCA :  Internal error, covariance matrix S is not symmetric!" << std::endl;
			ASSERT(false);
			exit(-1);
		}

		//new tjhImageWindow(title+" covar",S); //rh: , std::string title param removed

		// find the eigenvectors and values
		S.PCAonCovar(eigenvectors,eigenvalues);
	
	}

	//new tjhImageWindow(title+" eigenvectors",eigenvectors);
	//new tjhGraphWindow(title+" eigenvalues",eigenvalues);
}

void free_matrix(float **m, long nrl, long nrh, long ncl, long nch);
void free_vector(float *v, long nl, long nh);
float **matrix(long nrl, long nrh, long ncl, long nch);
float *vector(long nl, long nh); 

void CFloatMatrix::PCAonCovar(CFloatMatrix &eigenvectors,CFloatMatrix &eigenvalues) const
{
	// the matrix must be real and symmetric
	ASSERT(IsSymmetric());

	// the matrices for the eigenthings must be the right size
	eigenvectors.init(this->GetNRows(),this->GetNColumns());
    eigenvectors.MakeZero();
    eigenvalues.init(this->GetNRows(),1);
    eigenvalues.MakeZero();

	const int n=m_nrows;

	// no point proceeding if nothing to do
	if(n<1) return;

	// put the matrix into a form suitable for passing to jacobi
	float **a = matrix(1,n,1,n);
	float *d = vector(1,n);
	int r,c;
	for(r=0;r<n;r++)
		for(c=0;c<n;c++)
			a[r+1][c+1]=get(r,c);

	// perform the eigen-decomposition (code mativated by Numerical Recipes)

	float *e = vector(1,n);

	{
		//CTemporaryStatusText dummy("tred2 & tqli ... ");
		tred2(a,n,d,e);
		tqli(d,e,n,a);
		eigsrt(d,a,n);
	}

	// extract the eigenvalues from d
	int i;
	for(i=0;i<n;i++)
		eigenvalues.set(i,0,d[i+1]>0.0F?d[i+1]:0.0F);

	// extract the eigenvectors from a
	int j;
	for(i=0;i<n;i++)
		for(j=0;j<n;j++)
			eigenvectors.set(i,j,a[i+1][j+1]);

	// free the allocated memory
	free_matrix(a,1,n,1,n);
	free_vector(d,1,n);
	free_vector(e,1,n);
}

// friend operators (not actually members of CFloatMatrix)

CFloatMatrix operator*(CFloatMatrix a,float f)
{
	CFloatMatrix result(a);
	result*=f;
	return result;
}

CFloatMatrix operator*(float f,CFloatMatrix a)
{
	CFloatMatrix result(a);
	result*=f;
	return result;
}

CFloatMatrix operator*(CFloatMatrix a,CFloatMatrix b)
{
	// are the matrices compatible for multiplication?
	if(a.m_ncolumns==b.m_nrows)
	{
		// perform matrix multiplication
		CFloatMatrix result(a.m_nrows,b.m_ncolumns);
		result.MakeZero();
		int r,c,i;
		for(r=0;r<result.m_nrows;r++)
			for(c=0;c<result.m_ncolumns;c++)
				for(i=0;i<a.m_ncolumns;i++)
					result.set(r,c,result.get(r,c)+a.get(r,i)*b.get(i,c));

		return result;
	}
	else
	{
		ASSERT(false);
		return a;
	}
}

CFloatMatrix operator/(CFloatMatrix a,float f)
{
	CFloatMatrix result(a);
	result/=f;
	return result;
}

CFloatMatrix operator/(float f,CFloatMatrix a)
{
	CFloatMatrix result(a);
	result/=f;
	return result;
}

CFloatMatrix operator+(CFloatMatrix a,CFloatMatrix b)
{
	CFloatMatrix result(a);
	result+=b;
	return result;
}

CFloatMatrix operator-(CFloatMatrix a,CFloatMatrix b)
{
	CFloatMatrix result(a);
	result-=b;
	return result;
}

CFloatMatrix Transpose(CFloatMatrix a)
{
	CFloatMatrix result(a.m_ncolumns,a.m_nrows);

	// copy the values from a
	int r,c;
	for(r=0;r<result.m_nrows;r++)
		for(c=0;c<result.m_ncolumns;c++)
			result.set(r,c,a.get(c,r));

	return result;
}

CFloatMatrix Normalize(CFloatMatrix v)
{
	ASSERT(v.GetNColumns()==1);

	float length=0.0F;
	for(int i=0;i<v.m_nrows;i++)
		length+=v.m_data[i]*v.m_data[i];
	length = (float)sqrt(length);
	v/=length;

	return v;
}

#define ROTATE(a,i,j,k,l) g=a[i][j];h=a[k][l];a[i][j]=g-s*(h+g*tau);\
 a[k][l]=h+s*(g-h*tau);

//----------------------------------------------------------------------------------

#define NR_END 1
#define FREE_ARG char*

/* Computes all eigenvalues and eigenvectors of a real symmetric matrix a[1..n][1..n]. 
   On output, elements of a above the diagonal are destroyed. 
   d[1..n] returns the eigenvalues of a. v[1..n][1..n] is a matrix whose columns contain, 
   on output, the normalized eigenvectors of a. 
   nrot returns the number of Jacobi rotations that were required. */

void jacobi(float **a, int n, float d[], float **v, int *nrot) 
{ 
	int j,iq,ip,i; 
	float tresh,theta,tau,t,sm,s,h,g,c,*b,*z;

	b=vector(1,n); 
	z=vector(1,n); 
	for (ip=1;ip<=n;ip++) 
	{ 
		// Initialize to the identity matrix. 
		for (iq=1;iq<=n;iq++) v[ip][iq]=0.0F; 
		v[ip][ip]=1.0F; 
	} 
	for (ip=1;ip<=n;ip++) 
	{ 
		// Initialize b and d to the diagonal of a. 
		b[ip]=d[ip]=a[ip][ip]; 
		z[ip]=0.0F;	// This vector will accumulate terms of the form ta(pq as in equation (11.1.14). 
	} 
	*nrot=0; 
	for (i=1;i<=50;i++) 
	{ 
		sm=0.0F;
		for (ip=1;ip<=n-1;ip++) 
		{ 
			// Sum off-diagonal elements. 
			for (iq=ip+1;iq<=n;iq++) 
				sm += (float)fabs(a[ip][iq]); 
		} 
		if (sm == 0.0F) 
		{ 
			// The normal return, which relies on quadratic convergence to machine underflow. 
			free_vector(z,1,n); 
			free_vector(b,1,n); 
			return; 
		} 
		if (i < 4) 
			tresh=0.2F*sm/(n*n);		// ...on the first three sweeps. 
		else 
			tresh=0.0F; 				//...thereafter. 

		for (ip=1;ip<=n-1;ip++)
		{ 
			for (iq=ip+1;iq<=n;iq++) 
			{ 
				g=100.0F*(float)fabs(a[ip][iq]); 	// After four sweeps, skip the rotation if the off-diagonal element is small. 
				if(i > 4 &&(float)(fabs(d[ip])+g) == (float)fabs(d[ip]) 
					&& (float)(fabs(d[iq])+g) == (float)fabs(d[iq])) 
					a[ip][iq]=0.0;
				else if (fabs(a[ip][iq]) > tresh) 
				{ 
					h=d[iq]-d[ip]; 
					if ((float)(fabs(h)+g) == (float)fabs(h)) 
						t=(a[ip][iq])/h; 							// t = 1=(2*theta) 
					else 
					{ 
						theta=0.5F*h/(a[ip][iq]); 					// Equation (11.1.10). 
						t=1.0F/((float)fabs(theta)+(float)sqrt(1.0+theta*theta)); 
						if (theta < 0.0) t = -t; 
					} 
					c=1.0F/(float)sqrt(1+t*t); 
					s=t*c; 
					tau=s/(1.0F+c); 
					h=t*a[ip][iq]; 
					z[ip] -= h; 
					z[iq] += h; 
					d[ip] -= h; 
					d[iq] += h; 
					a[ip][iq]=0.0; 
					for (j=1;j<=ip-1;j++) 
					{ 
						// Case of rotations 1 <= j <p. 
						ROTATE(a,j,ip,j,iq) 
					} 
					for (j=ip+1;j<=iq-1;j++) 
					{ 
						// Case of rotations p <j<q. 
						ROTATE(a,ip,j,j,iq) 
					} 
					for (j=iq+1;j<=n;j++) 
					{ 
						// Case of rotations q <j <= n. 
						ROTATE(a,ip,j,iq,j) 
					} 
					for (j=1;j<=n;j++) 
					{ 
						ROTATE(v,j,ip,j,iq) 
					} 
					++(*nrot); 
				} 
			} 
		} 
		for (ip=1;ip<=n;ip++) 
		{ 
			b[ip] += z[ip]; 
			d[ip]=b[ip]; 			// Update d with the sum of tapq, and reinitialize z. 
			z[ip]=0.0; 
		} 
	} 
	//nrerror("Too many iterations in routine jacobi"); 
	// it is bad if program execution reaches here
	ASSERT(false);
	exit(-1);
}

//----------------------------------------------------------------------------------

/* Given the eigenvalues d[1..n] and eigenvectors v[1..n][1..n] as output from 
   jacobi ( x 11.1) or tqli ( x 11.3), this routine sorts the eigenvalues into descending order, 
   and rearranges the columns of v correspondingly. The method is straight insertion. */

void eigsrt(float d[], float **v, int n)
{ 

	int k,j,i; 
	float p;

	for (i=1;i<n;i++) { 
		p=d[k=i];
		for (j=i+1;j<=n;j++) 
			if (d[j] >= p) p=d[k=j]; 
		if (k != i) { 
			d[k]=d[i]; 
			d[i]=p; 
			for (j=1;j<=n;j++) { 
				p=v[j][i]; 
				v[j][i]=v[j][k]; 
				v[j][k]=p; 
			} 
		} 
	} 
}

float *vector(long nl, long nh) 	/* allocate a float vector with subscript range v[nl..nh] */ 
{ 
	float *v;

	v=(float *)malloc((size_t) ((nh-nl+1+NR_END)*sizeof(float))); 
	if (!v) exit(-1); //nrerror("allocation failure in vector()"); 
	return v-nl+NR_END; 

}

float **matrix(long nrl, long nrh, long ncl, long nch)
	/* allocate a float matrix with subscript range m[nrl..nrh][ncl..nch] */
{
	long i, nrow=nrh-nrl+1,ncol=nch-ncl+1;
	float **m;

	/* allocate pointers to rows */
	m=(float **) malloc((size_t)((nrow+NR_END)*sizeof(float*)));
	if (!m) exit(-1);//nrerror("allocation failure 1 in matrix()");
	m += NR_END;
	m -= nrl;

	/* allocate rows and set pointers to them */
	m[nrl]=(float *) malloc((size_t)((nrow*ncol+NR_END)*sizeof(float)));
	if (!m[nrl]) exit(-1);//nrerror("allocation failure 2 in matrix()");
	m[nrl] += NR_END;
	m[nrl] -= ncl;
	for(i=nrl+1;i<=nrh;i++) m[i]=m[i-1]+ncol;

	/* return pointer to array of pointers to rows */
	return m;
}

double **matrix_double(long nrl, long nrh, long ncl, long nch)
	/* allocate a float matrix with subscript range m[nrl..nrh][ncl..nch] */
{
	long i, nrow=nrh-nrl+1,ncol=nch-ncl+1;
	double **m;

	/* allocate pointers to rows */
	m=(double **) malloc((size_t)((nrow+NR_END)*sizeof(double*)));
	if (!m) exit(-1);//nrerror("allocation failure 1 in matrix()");
	m += NR_END;
	m -= nrl;

	/* allocate rows and set pointers to them */
	m[nrl]=(double *) malloc((size_t)((nrow*ncol+NR_END)*sizeof(double)));
	if (!m[nrl]) exit(-1);//nrerror("allocation failure 2 in matrix()");
	m[nrl] += NR_END;
	m[nrl] -= ncl;
	for(i=nrl+1;i<=nrh;i++) m[i]=m[i-1]+ncol;

	/* return pointer to array of pointers to rows */
	return m;
}

void free_vector(float *v, long nl, long nh) 	/* free a float vector allocated with vector() */ 
{ 
	free((FREE_ARG) (v+nl-NR_END)); 
}

void free_matrix(float **m, long nrl, long nrh, long ncl, long nch)
	/* free a float matrix allocated by matrix() */
{
	free((FREE_ARG) (m[nrl]+ncl-NR_END));
	free((FREE_ARG) (m+nrl-NR_END));
}

void free_matrix_double(double **m, long nrl, long nrh, long ncl, long nch)
	/* free a float matrix allocated by matrix() */
{
	free((FREE_ARG) (m[nrl]+ncl-NR_END));
	free((FREE_ARG) (m+nrl-NR_END));
}

bool IsFiniteNumber(float x) 
{
	return (x <= FLT_MAX && x >= -FLT_MAX)	; 
}    


float DotProduct(CFloatMatrix a,CFloatMatrix b)
{
	// scalar multiply the matrices and return the sum of their entries
	// matrices must be same size
	if((a.m_nrows==b.m_nrows)&&(a.m_ncolumns==b.m_ncolumns))
	{
		float total=0.0F;
		for(int i=0;i<a.m_nrows*a.m_ncolumns;i++)
		{
			float aF = a.m_data[i];
			float bF = b.m_data[i];
			if(IsFiniteNumber(aF) && IsFiniteNumber(bF))
				total += aF*bF;
		}
		return total;
	}
	else 
	{
		ASSERT(false);
		return 0;
	}
}


C2dVector Normalized(C2dVector a)
{
	return a.Normalize();
}

float CFloatMatrix::SumOfSquares()
{
	float result=0.0F;
	for(int i=0;i<m_nrows*m_ncolumns;i++)
		result += m_data[i]*m_data[i];
	return result;
}

void tred2(float **a, int n, float d[], float e[])
/*Householder reduction of a real, symmetric matrix a[1..n][1..n]. On output, a is replaced
by the orthogonal matrix Q effecting the transformation. d[1..n] returns the diagonal ele-
ments of the tridiagonal matrix, and e[1..n] the off-diagonal elements, with e[1]=0. Several
statements, as noted in comments, can be omitted if only eigenvalues are to be found, in which
case a contains no useful information on output. Otherwise they are to be included.
*/
{
	int l,k,j,i;
	float scale,hh,h,g,f;

	for (i=n;i>=2;i--) {
		l=i-1;
		h=scale=0.0;
		if(l > 1) {
			for (k=1;k<=l;k++)
				scale += (float)fabs(a[i][k]);
			if (scale == 0.0) //Skip transformation.
				e[i]=a[i][l];
			else {
				for (k=1;k<=l;k++) {
					a[i][k] /= scale; //Use scaled a's for transformation.
					h += a[i][k]*a[i][k]; //Form . in h.
				}
				f=a[i][l];
				g=(f >= 0.0 ? -(float)sqrt(h) : (float)sqrt(h));
				e[i]=scale*g;
				h -= f*g; //Now h is equation (11.2.4).
				a[i][l]=f-g; //Store u in the ith row of a.
				f=0.0;
				for (j=1;j<=l;j++) {
					/* Next statement can be omitted if eigenvectors not wanted */
					a[j][i]=a[i][j]/h; //Store u=H in ith column of a.
					g=0.0; //Form an element of A . u in g.
					for (k=1;k<=j;k++)
						g += a[j][k]*a[i][k];
					for (k=j+1;k<=l;k++)
						g += a[k][j]*a[i][k];
					e[j]=g/h; //Form element of p in temporarily unused element of e.
					f += e[j]*a[i][j];
				}
				hh=f/(h+h); //Form K, equation (11.2.11).
				for (j=1;j<=l;j++) { //Form q and store in e overwriting p.
					f=a[i][j];
					e[j]=g=e[j]-hh*f;
					for (k=1;k<=j;k++) //Reduce a, equation (11.2.13).
						a[j][k] -= (f*e[k]+g*a[i][k]);
				}
			}
		} else
			e[i]=a[i][l];
		d[i]=h;
	}
	/* Next statement can be omitted if eigenvectors not wanted */
	d[1]=0.0;
	e[1]=0.0;
	/* Contents of this loop can be omitted if eigenvectors not
					wanted except for statement d[i]=a[i][i]; */
	for (i=1;i<=n;i++) { //Begin accumulation of transformation matrices. 
		l=i-1;
		if (d[i]) { //This block skipped when i=1.
			for (j=1;j<=l;j++) {
				g=0.0;
				for (k=1;k<=l;k++) //Use u and u=H stored in a to form P . Q.
					g += a[i][k]*a[k][j];
				for (k=1;k<=l;k++)
					a[k][j] -= g*a[k][i];
			}
		}
		d[i]=a[i][i]; //This statement remains.
		a[i][i]=1.0; //Reset row and column of a to identity matrix for next iteration. 
		for (j=1;j<=l;j++) a[j][i]=a[i][j]=0.0;
	}
}

#define SIGN(a,b) ((b) >= 0.0 ? fabs(a) : -fabs(a))

static float sqrarg;
#define SQR(a) ((sqrarg=(a)) == 0.0 ? 0.0 : sqrarg*sqrarg)

float pythag(float a, float b)
//Computes (a 2 + b 2 ) 1=2 without destructive underflow or overflow.
{
	float absa,absb;

	absa=(float)fabs(a);
	absb=(float)fabs(b);
	if (absa > absb) return absa*(float)sqrt(1.0+SQR(absb/absa));
	else return (absb == 0.0 ? 0.0F : absb*(float)sqrt(1.0+SQR(absa/absb)));
}

void tqli(float d[], float e[], int n, float **z)
/*QL algorithm with implicit shifts, to determine the eigenvalues and eigenvectors of a real, sym-
metric, tridiagonal matrix, or of a real, symmetric matrix previously reduced by tred2 (section 11.2). On
input, d[1..n] contains the diagonal elements of the tridiagonal matrix. On output, it returns
the eigenvalues. The vector e[1..n] inputs the subdiagonal elements of the tridiagonal matrix,
with e[1] arbitrary. On output e is destroyed. When finding only the eigenvalues, several lines
may be omitted, as noted in the comments. If the eigenvectors of a tridiagonal matrix are de-
sired, the matrix z[1..n][1..n] is input as the identity matrix. If the eigenvectors of a matrix
that has been reduced by tred2 are required, then z is input as the matrix output by tred2.
In either case, the kth column of z returns the normalized eigenvector corresponding to d[k].
*/
{
	float pythag(float a, float b);
	int m,l,iter,i,k;
	float s,r,p,g,f,dd,c,b;

	for (i=2;i<=n;i++) e[i-1]=e[i]; //Convenient to renumber the elements of e. 
	e[n]=0.0;
	for (l=1;l<=n;l++) {
		iter=0;
		do {
			for (m=l;m<=n-1;m++) { // Look for a single small subdiagonal element to split the matrix.
				dd=(float)(fabs(d[m])+fabs(d[m+1]));
				if ((float)(fabs(e[m])+dd) == dd) break;
			}
			if (m != l) {
				if (iter++ == 300) {
					//nrerror("Too many iterations in tqli");
					//ASSERT(false);
					std::cerr << "In CFloatMatrix.cpp::tqli (QL algorithm):  Too many iterations in tqli! Crashing out..." << std::endl; 
					return;
				}
				g=(d[l+1]-d[l])/(2.0F*e[l]); //Form shift.
				r=pythag(g,1.0);
				g=d[m]-d[l]+e[l]/(g+(float)SIGN(r,g)); //This is dm - ks
				s=c=1.0;
				p=0.0;
				for (i=m-1;i>=l;i--) { //A plane rotation as in the original QL, followed by Givens rotations to restore tridiagonal form.
					f=s*e[i];
					b=c*e[i];
					e[i+1]=(r=pythag(f,g));
					if (r == 0.0) { //Recover from underflow.
						d[i+1] -= p;
						e[m]=0.0;
						break;
					}
					s=f/r;
					c=g/r;
					g=d[i+1]-p;	
					r=(d[i]-g)*s+2.0F*c*b;
					d[i+1]=g+(p=s*r);
					g=c*r-b;
					/* Next loop can be omitted if eigenvectors not wanted*/
					for (k=1;k<=n;k++) { //Form eigenvectors.
						f=z[k][i+1];
						z[k][i+1]=s*z[k][i]+c*f;
						z[k][i]=c*z[k][i]-s*f;
					}
				}
				if (r == 0.0 && i >= l) continue;
				d[l] -= p;
				e[l]=g;
				e[m]=0.0;
			}
		} while (m != l);
	}
}

bool CFloatMatrix::IsSymmetric() const
{
	// must be square for a start
	if(m_nrows != m_ncolumns) return false;

	// entries must tally with their reflection in the diagonal
	int r,c;
	for(c=0;c<m_ncolumns;c++)
		for(r=0;r<m_nrows;r++)
			if(get(r,c)!=get(c,r)) return false;

	return true;
}

CFloatMatrix SqrEntries(CFloatMatrix a)
{
	CFloatMatrix result(a);
	for(int i=0;i<a.m_nrows*a.m_ncolumns;i++) result.m_data[i]=sqr(a.m_data[i]);
	return result;
}

 // version that uses vtk's matrix inversion code
CFloatMatrix Inverse(CFloatMatrix a)
{
	const int n = a.GetNRows();
	ASSERT(n==a.GetNColumns());

	double **in = matrix_double(0,n-1,0,n-1);
	double **out = matrix_double(0,n-1,0,n-1);
	int r,c;
	for(r=0;r<n;r++)
	{
		for(c=0;c<n;c++)
			in[r][c]=a.get(r,c);
	}

	vtkMath::InvertMatrix(in,out,n);

	for(r=0;r<n;r++)
	{
		for(c=0;c<n;c++)
			a.set(r,c,out[r][c]);
	}

	free_matrix_double(in,0,n-1,0,n-1);
	free_matrix_double(out,0,n-1,0,n-1);

	return a;
}

int *ivector(long nl, long nh)
/* allocate an int vector with subscript range v[nl..nh] */
{
	int *v;
	v=(int *)malloc((size_t) ((nh-nl+1+NR_END)*sizeof(int)));
	if (!v) ASSERT(false);//nrerror("allocation failure in ivector()");
	return v-nl+NR_END;
}

void free_ivector(int *v, long nl, long nh)
/* free an int vector allocated with ivector() */
{
	free((FREE_ARG) (v+nl-NR_END));
}

#define SWAP(a,b) {temp=(a);(a)=(b);(b)=temp;}
void gaussj(float **a, int n, float **b, int m)

/* Linear equation solution by Gauss-Jordan elimination, equation (2.1.1) above. 
a[1..n][1..n] is the input matrix. b[1..n][1..m] is input containing the 
m right-hand side vectors. On output, a is replaced by its matrix inverse, 
and b is replaced by the corresponding set of solution vectors. */

{
	int *indxc,*indxr,*ipiv;
	int i,icol,irow,j,k,l,ll;
	float big,dum,pivinv,temp;

	indxc=ivector(1,n); // The integer arrays ipiv, indxr, and indxc are used for bookkeeping on the pivoting.
	indxr=ivector(1,n);
	ipiv=ivector(1,n);
	for (j=1;j<=n;j++) ipiv[j]=0;
	for (i=1;i<=n;i++) { // This is the main loop over the columns to be reduced. 
		big=0.0;
		for (j=1;j<=n;j++) // This is the outer loop of the search for a pivot element. 
			if (ipiv[j] != 1)
				for (k=1;k<=n;k++) {
					if (ipiv[k] == 0) {
						if (fabs(a[j][k]) >= big) {
							big=fabs(a[j][k]);
							irow=j;
							icol=k;
						}
					} else if (ipiv[k] > 1) ASSERT(false);//nrerror("gaussj: Singular Matrix-1");
				}
				++(ipiv[icol]);
				/*We now have the pivot element, so we interchange rows, if needed, to put the pivot
				element on the diagonal. The columns are not physically interchanged, only relabeled:
				indxc[i], the column of the ith pivot element, is the ith column that is reduced, while
				indxr[i] is the row in which that pivot element was originally located. 
				If indxr[i] != indxc[i] there is an implied column interchange. With this form of bookkeeping, the
				solution b's will end up in the correct order, and the inverse matrix will be scrambled
				by columns.*/
				if (irow != icol) {
					for (l=1;l<=n;l++) SWAP(a[irow][l],a[icol][l]);
					for (l=1;l<=m;l++) SWAP(b[irow][l],b[icol][l]);
				}
				indxr[i]=irow; /*We are now ready to divide the pivot row by the
									pivot element, located at irow and icol. */
				indxc[i]=icol;
				if (a[icol][icol] == 0.0) ASSERT(false);//nrerror("gaussj: Singular Matrix-2");
				pivinv=1.0/a[icol][icol];
				a[icol][icol]=1.0;
				for (l=1;l<=n;l++) a[icol][l] *= pivinv;
				for (l=1;l<=m;l++) b[icol][l] *= pivinv;
				for (ll=1;ll<=n;ll++) // Next, we reduce the rows...
					if (ll != icol) 
					{ // ...except for the pivot one, of course.
						dum=a[ll][icol];
						a[ll][icol]=0.0;
						for (l=1;l<=n;l++) a[ll][l] -= a[icol][l]*dum;
						for (l=1;l<=m;l++) b[ll][l] -= b[icol][l]*dum;
					}
	}
	/* This is the end of the main loop over columns of the reduction. It only remains to unscram-
	ble the solution in view of the column interchanges. We do this by interchanging pairs of
	columns in the reverse order that the permutation was built up. */
	for (l=n;l>=1;l--) {
		if (indxr[l] != indxc[l])
			for (k=1;k<=n;k++)
				SWAP(a[k][indxr[l]],a[k][indxc[l]]);
	} // And we are done.
	free_ivector(ipiv,1,n);
	free_ivector(indxr,1,n);
	free_ivector(indxc,1,n);
}

/*
//rh: presumably this was used only for debugging purposes ...  removed from public interface
void CFloatMatrix::Serialize(CArchive &ar)
{
	if(ar.IsLoading())
	{
		if(m_data!=NULL) delete []m_data;

		ar >> m_nrows >> m_ncolumns;
		m_data = new float[m_nrows*m_ncolumns];

		unsigned char value;
		for(int i=0;i<m_nrows*m_ncolumns;i++)
		{
			ar >> value;
			m_data[i] = float(value)/255.0F;
		}
	}
	else // write the data to file
	{
		ar << m_nrows << m_ncolumns;
		for(int i=0;i<m_nrows*m_ncolumns;i++)
			ar << (unsigned char)(m_data[i]*255.0F);
	}
}
*/

void CFloatMatrix::ConvertQuaternion(CFloatMatrix q)
{
	// matrices must be the right size
	if(m_nrows!=4 || m_ncolumns!=4 || q.GetNRows()!=4 || q.GetNColumns()!=1) 
		return;

	// --- have found two conversion between quaternions and matrices, which to use!? ---

	if(true)
	{
		// from Stanford's ZipPack
		float q00,q01,q02,q03;
		float q11,q12,q13;
		float q22,q23;
		float q33;

		q00 = q.get(0,0)*q.get(0,0);
		q01 = q.get(0,0)*q.get(1,0);
		q02 = q.get(0,0)*q.get(2,0);
		q03 = q.get(0,0)*q.get(3,0);

		q11 = q.get(1,0)*q.get(1,0);
		q12 = q.get(1,0)*q.get(2,0);
		q13 = q.get(1,0)*q.get(3,0);

		q22 = q.get(2,0)*q.get(2,0);
		q23 = q.get(2,0)*q.get(3,0);

		q33 = q.get(3,0)*q.get(3,0);

		set(0,0,q00+q11-q22-q33);
		set(0,1,2*(q12-q03));
		set(0,2,2*(q13+q02));
		set(0,3,0.0);

		set(1,0,2*(q12+q03));
		set(1,1,q00+q22-q11-q33);
		set(1,2,2*(q23-q01));
		set(1,3,0.0);

		set(2,0,2*(q13-q02));
		set(2,1,2*(q23+q01));
		set(2,2,q00+q33-q11-q22);
		set(2,3,0.0);

		set(3,0,0.0);
		set(3,1,0.0);
		set(3,2,0.0);
		set(3,3,1.0);
	}
	else
	{
		// from an online FAQ
		float X,Y,Z,W;
		X = q.get(0,0);
		Y = q.get(1,0);
		Z = q.get(2,0);
		W = q.get(3,0);

		float xx,xy,xz,xw,yy,yz,yw,zz,zw;
		xx = X*X;
		xy = X*Y;
		xz = X*Z;
		xw = X*W;
		yy = Y*Y;
		yz = Y*Z;
		yw = Y*W;
		zz = Z*Z;
		zw = Z*W;

		set(0,0,1.0-2.0*(yy+zz));
		set(0,1,2.0*(xy-zw));
		set(0,2,2.0*(xz+yw));
		set(0,3,0.0);

		set(1,0,2.0*(xy+zw));
		set(1,1,1.0-2.0*(xx+zz));
		set(1,2,2.0*(yz-xw));
		set(1,3,0.0);

		set(2,0,2.0*(xz-yw));
		set(2,1,2.0*(yz+xw));
		set(2,2,1.0-2.0*(xx+yy));
		set(2,3,0.0);

		set(3,0,0.0);
		set(3,1,0.0);
		set(3,2,0.0);
		set(3,3,1.0);
	}
}