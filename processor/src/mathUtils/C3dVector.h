// --- C3dVector.h ----------------------------------------------

#ifndef C3DVECTOR
#define C3DVECTOR

/// A simple class to manage a 3D point/vector, with coordinates (x,y,z)
class C3dVector
{
	public:
		// data members
		float x,y,z;
	public:
		// default constructor
		C3dVector(float a=0.0F,float b=0.0F,float c=0.0F);
		// alternative constructor
		C3dVector(int a,int b,int c);
		// Begin MDD 19-JAN-2006 Added for VTK 4.4
		C3dVector(double a,double b,double c);
	
		// copy constructor
		C3dVector(const C3dVector& v);
		// operators
		void Normalize();
		float Length() const;
		float Length2() const;
		// unary operators
		C3dVector& operator=(const C3dVector& v);
		C3dVector& operator+=(const C3dVector& v);
		C3dVector& operator-=(const C3dVector& v);
		C3dVector& operator*=(float f);
		C3dVector& operator/=(float f);
		bool operator==(const C3dVector& v) const
			{ return((x==v.x)&&(y==v.y)&&(z==v.z)); }
		friend C3dVector Normalize(const C3dVector& v);
		void RotateXAbout(C3dVector p,float radians);
		void RotateYAbout(C3dVector p,float radians);
		void RotateZAbout(C3dVector p,float radians);
		void RotateAboutAxisRH(C3dVector base,C3dVector axis,float radians);
		// binary operators
		friend C3dVector operator*(C3dVector a,float f);
		friend C3dVector operator*(float f,C3dVector a);
		friend C3dVector operator/(C3dVector a,float f);
		friend C3dVector operator/(float f,C3dVector a);
		friend C3dVector operator+(C3dVector a,C3dVector b);
		
		friend C3dVector operator-(C3dVector a,C3dVector b);
		 
		friend float DotProduct(C3dVector a,C3dVector b);
        friend C3dVector ElementSqr(C3dVector a);
        friend C3dVector ElementSqrt(C3dVector a);
		friend float Dot(C3dVector a,C3dVector b) { return DotProduct(a,b); }
		friend C3dVector CrossProduct(C3dVector a,C3dVector b);
		friend float Length(C3dVector v) { return v.Length(); }
		friend float Length2(C3dVector v) { return v.Length2(); }
		friend float Dist(C3dVector a, C3dVector b);
};

#endif // C3DVECTOR

//-----------------------------------------------------------