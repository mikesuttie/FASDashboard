// --- C3dVector.cpp -------------------------------------------

#include "C3dVector.h"
#include <math.h>
#include "C2dVector.h"
 

// default constructor
C3dVector::C3dVector(float a,float b,float c)
: x(a), y(b), z(c) {}

// alternative constructors
C3dVector::C3dVector(int a,int b,int c)
: x((float)a), y((float)b), z((float)c) {}

// Begin MDD 19-JAN-2006 Added for VTK 4.4
C3dVector::C3dVector(double a,double b,double c)
: x((float)a), y((float)b), z((float)c) {}
// End MDD 19-JAN-2006 Added for VTK 4.4


// copy constructor
C3dVector::C3dVector(const C3dVector& v)
: x(v.x), y(v.y), z(v.z) {}

// overloaded unary operators
C3dVector& C3dVector::operator=(const C3dVector& v)
{
	x=v.x; y=v.y; z=v.z;
	return(*this);
}
C3dVector& C3dVector::operator+=(const C3dVector& v)
{
	x+=v.x; y+=v.y; z+=v.z;
	return(*this);
}
C3dVector& C3dVector::operator-=(const C3dVector& v)
{
	x-=v.x; y-=v.y; z-=v.z;
	return(*this);
}
C3dVector& C3dVector::operator*=(float f)
{
	x*=f; y*=f; z*=f;
	return(*this);
}
C3dVector& C3dVector::operator/=(float f)
{
	x/=f; y/=f; z/=f;
	return(*this);
}
float C3dVector::Length() const
{
	return((float)sqrt(x*x+y*y+z*z));
}
float C3dVector::Length2() const
{
	return(x*x+y*y+z*z);
}
void C3dVector::Normalize()
{
	float length = Length();
	if(length!=0.0F)
	{
		x/=length;
		y/=length;
		z/=length;
	}
}
C3dVector Normalize(const C3dVector& v)
{
	C3dVector r = v;
	float length = r.Length();
	if(length!=0.0F)
	{
		r.x/=length;
		r.y/=length;
		r.z/=length;
	}
	return(r);
}

// overloaded binary operators
C3dVector operator*(C3dVector a,float f)
{
	return C3dVector(a.x*f,a.y*f,a.z*f);
}
C3dVector operator*(float f,C3dVector a)
{
	return C3dVector(a*f);  // call other multiplication function
}
C3dVector operator/(C3dVector a,float f)
{
	return C3dVector(a.x/f,a.y/f,a.z/f);
}
C3dVector operator/(float f,C3dVector a)
{
	return C3dVector(a/f);  // call other divide function
}
C3dVector operator+(C3dVector a,C3dVector b)
{
	return C3dVector(a.x+b.x,a.y+b.y,a.z+b.z);
}
C3dVector operator-(C3dVector a,C3dVector b)
{
	return C3dVector(a.x-b.x,a.y-b.y,a.z-b.z);
}
float DotProduct(C3dVector a,C3dVector b)
{
	return(a.x*b.x + a.y*b.y + a.z*b.z);
}
C3dVector ElementSqr(C3dVector a)
{
    return C3dVector(a.x*a.x,a.y*a.y,a.z*a.z);
}
C3dVector ElementSqrt(C3dVector a)
{
    return C3dVector((float)sqrt(a.x),(float)sqrt(a.y),(float)sqrt(a.z));
}
C3dVector CrossProduct(C3dVector a,C3dVector b)
{
	C3dVector c;
	c.x = a.y*b.z - a.z*b.y;
	c.y = a.z*b.x - a.x*b.z;
	c.z = a.x*b.y - a.y*b.x;
	return(c);
}

float Dist(C3dVector a, C3dVector b)
{

	return sqrtf((a.x-b.x) * (a.x - b.x) + (a.y-b.y) * (a.y - b.y) + (a.z-b.z) * (a.z - b.z));
}
 


//------------------------------------------------
void C3dVector::RotateXAbout(C3dVector p,float radians)
{
	// find sine and cosine of the angle
	float sine = (float)sin(radians);
	float cosine = (float)cos(radians);

	// rotate the eye around the look_at point
	C3dVector new_pos = *this - p;
	float temp = cosine*new_pos.y - sine*new_pos.z;
	new_pos.z = sine*new_pos.y + cosine*new_pos.z;
	new_pos.y = temp;
	*this = p + new_pos;
}
void C3dVector::RotateYAbout(C3dVector p,float radians)
{
	// find sine and cosine of the angle
	float sine = (float)sin(radians);
	float cosine = (float)cos(radians);

	// rotate the eye around the look_at point
	C3dVector new_pos = *this - p;
	float temp = cosine*new_pos.x - sine*new_pos.z;
	new_pos.z = sine*new_pos.x + cosine*new_pos.z;
	new_pos.x = temp;
	*this = p + new_pos;
}
void C3dVector::RotateZAbout(C3dVector p,float radians)
{
	// find sine and cosine of the angle
	float sine = (float)sin(radians);
	float cosine = (float)cos(radians);

	// rotate the eye around the look_at point
	C3dVector new_pos = *this - p;
	float temp = cosine*new_pos.x - sine*new_pos.y;
	new_pos.y = sine*new_pos.x + cosine*new_pos.y;
	new_pos.x = temp;
	*this = p + new_pos;
}
void C3dVector::RotateAboutAxisRH(C3dVector base,C3dVector axis,float radians)
{
	// rotate about the given axis using a right-hand grip
	C3dVector p3d = *this;

	// find centre of rotation plane for the point
	C3dVector origin = base + axis * DotProduct(axis,p3d-base);

	// find two base axes of plane
	C3dVector p_axis_x = p3d-origin;
	C3dVector p_axis_y = CrossProduct(axis,p_axis_x);
	p_axis_x.Normalize();
	p_axis_y.Normalize();

	// convert point's location into plane coordinates
	C2dVector p2d;
	p2d.x = DotProduct(p3d-origin,p_axis_x);
	p2d.y = DotProduct(p3d-origin,p_axis_y);
	// (centre of plane is (0,0) in plane coords)

	// rotate point about origin in plane coords
	float sine = (float)sin(radians);
	float cosine = (float)cos(radians);
	float temp = cosine*p2d.x - sine*p2d.y;
	p2d.y = sine*p2d.x + cosine*p2d.y;
	p2d.x = temp;

	// convert back into 3d space coordinates
	*this = origin + p_axis_x*p2d.x + p_axis_y*p2d.y;
}