#ifndef GEOMETRYFUNCTIONS_H
#define GEOMETRYFUNCTIONS_H

#include "../mathUtils/C3dVector.h"

#include "vtkMath.h"

namespace geom {

	// Already defined in C3dVector
	/*
	float DotProduct(C3dVector a, C3dVector b)
	{
		return(a.x * b.x + a.y * b.y + a.z * b.z);
	}

	C3dVector CrossProduct(C3dVector a, C3dVector b)
	{
		C3dVector c;
		c.x = a.y * b.z - a.z * b.y;
		c.y = a.z * b.x - a.x * b.z;
		c.z = a.x * b.y - a.y * b.x;
		return(c);
	}
	*/

	double DistanceBetweenLines(
		double l0[3], double l1[3],                 // line 1
		double m0[3], double m1[3],                 // line 2
		double closestPt1[3], double closestPt2[3], // closest points
		double& t1, double& t2) // parametric coords of the closest points
	{
		// Part of this function was adapted from "GeometryAlgorithms.com"
		//
		// Copyright 2001, softSurfer (www.softsurfer.com)
		// This code may be freely used and modified for any purpose
		// providing that this copyright notice is included with it.
		// SoftSurfer makes no warranty for this code, and cannot be held
		// liable for any real or imagined damage resulting from its use.
		// Users of this code must verify correctness for their application.

		const double u[3] = { l1[0] - l0[0], l1[1] - l0[1], l1[2] - l0[2] };
		const double v[3] = { m1[0] - m0[0], m1[1] - m0[1], m1[2] - m0[2] };
		const double w[3] = { l0[0] - m0[0], l0[1] - m0[1], l0[2] - m0[2] };
		const double    a = vtkMath::Dot(u, u);
		const double    b = vtkMath::Dot(u, v);
		const double    c = vtkMath::Dot(v, v);        // always >= 0
		const double    d = vtkMath::Dot(u, w);
		const double    e = vtkMath::Dot(v, w);
		const double    D = a * c - b * b;       // always >= 0

		// compute the line parameters of the two closest points
		if (D < 1e-6)
		{         // the lines are almost parallel
			t1 = 0.0;
			t2 = (b > c ? d / b : e / c);   // use the largest denominator
		}
		else
		{
			t1 = (b * e - c * d) / D;
			t2 = (a * e - b * d) / D;
		}

		for (unsigned int i = 0; i < 3; i++)
		{
			closestPt1[i] = l0[i] + t1 * u[i];
			closestPt2[i] = m0[i] + t2 * v[i];
		}

		// Return the distance squared between the lines = 
		// the mag squared of the distance between the two closest points 
		// = L1(t1) - L2(t2)
		return vtkMath::Distance2BetweenPoints(closestPt1, closestPt2);
	}

} // namespace

#endif // GEOMETRYFUNCTIONS_H