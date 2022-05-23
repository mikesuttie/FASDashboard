#ifndef __vtkSurfacePCA_h
#define __vtkSurfacePCA_h

#include <vtkAlgorithm.h>

#include <vtkDoubleArray.h>
#include <vtkSmartPointer.h>
#include <vtkThinPlateSplineTransform.h> // for param of fct. ApplyResampleFilter

#include "vtkPCAModel.h"

class vtkPolyData;
class vtkPointSet;

/*
  This class represents a dense surface model. There are two ways to create it:
  1. Specify the input surfaces and compute the model, using Execute()
  2. Load an existing model file
  3. Compute the matrices yourself and pass them in directly, bypassing Execute()
*/

/* TODO( rh ): Is this vtkTypeRevisionMacro required? If so, replace with vtkTypeMacro.
	// ... Defined in vtkSetGet.h:
	// Version of vtkTypeMacro that adds the CollectRevisions method.
	// This version takes a third argument that may be VTK_COMMON_EXPORT,
	// VTK_FILTERING_EXPORT, etc. You should not use this version unless you
	// have split the implementation of a class across multiple VTK libraries.
	// When in doubt, use vtkTypeRevisionMacro instead.
	#define vtkExportedTypeRevisionMacro(thisClass,superclass,dllExport) \
	  protected: \
	  dllExport void CollectRevisions(ostream& os); \
	  public: \
	  vtkTypeMacro(thisClass,superclass)

vtkTypeRevisionMacro(vtkSurfacePCA,vtkAlgorithm);
*/

class vtkSurfacePCA : public vtkPCAModel, public vtkAlgorithm
{
public:

	// Prints information about the state of the filter.
	void PrintSelf(ostream& os, vtkIndent indent);

	// Creates with similarity transform.
	static vtkSurfacePCA* New();

	// ----- functions to use when computing the model from scratch -----

	// Return true if file specified by path 'model_File' exists and was loaded correctly, false otherwise
	bool LoadFile(std::string model_FileName);

	// Return number of landmarks in face model
	int Getnlandmarks() { return this->n_landmarks; }

	// for an unseen surface, return the parameters that best model it
	void GetApproximateShapeParameters(vtkPolyData* shape, vtkPointSet* landmarks,
		vtkDoubleArray* b, int rigid_body = true);

	// Take vector b (input param) and apply it to mean shape. Output param: signature.
	void ParameteriseShape(vtkSmartPointer<vtkDoubleArray> b, vtkSmartPointer<vtkPolyData> shape);

	// Fills the landmark with point using nearest cellID from mean shape
	void GetParameterisedLandmarks(vtkPolyData* shape, vtkPolyData* landmarks);

	// Description:
	  // Fills the shape with:
	  //
	  // mean + b[0] * sqrt(eigenvalue[0]) * eigenvector[0]
	  //      + b[1] * sqrt(eigenvalue[1]) * eigenvector[1]
	  // ...
	  //      + b[sizeb-1] * sqrt(eigenvalue[bsize-1]) * eigenvector[bsize-1]
	  //
	  // here b are the parameters expressed in standard deviations
	  // bsize is the number of parameters in the b vector
	  // Allocate shape using vtkPolyData::New before using this function.
	void GetParameterisedShape(vtkDoubleArray* b, vtkPolyData* shape); //consumed in msNormalisationTools.cpp

	  // Retrieve how many modes there are available (may not be s-1 since we typically only store 98%)
	int GetTotalNumModes() { return this->Evals->GetNumberOfTuples(); } //consumed in msNormalisationTools.h

	void Resample(vtkPolyData* in, vtkPointSet* landmarks, vtkPolyData* out);


private:
	// Function resamples mesh passes through param 'in' to topology of mean mesh. Invoked by function 'Resample'.
	void ApplyResampleSurfaceFilter(vtkSmartPointer<vtkPolyData> in, vtkSmartPointer<vtkPolyData> outputMesh,
		vtkSmartPointer<vtkPolyData> mean_surface, vtkSmartPointer<vtkThinPlateSplineTransform> tps);

	// Mean landmarks: the original sparse ones
	void GetMeanLandmarks(vtkPolyData* landmarks);

	// Pass-in the current landmarks (pseudo generatated landmarks)
	void SetCurrentLandmarks(vtkPolyData* landmarks);
	void GetCurrentLandmarks(vtkPolyData* landmarks);

	double* GetMeanshape() { return this->meanshape; }

	// Retrieve the input with index idx (usually only used for pipeline tracing).
	vtkPolyData* GetInput(int idx);

	// Retrieve the mode values needed to model input i (only valid after Update)
	void GetModesForInput(int i, vtkDoubleArray* b, int b_size);

	void Update();

	// Get the vector of eigenvalues sorted in descending order
	vtkDoubleArray* GetEvals() { return this->Evals; }
	float* GetMeanLandmarks() { return this->mean_landmarks; }
	double** GetEvecMat2() { return this->evecMat2; }

	// As GetParameteriseShape, but vector b is empty. So, just write meanshape into out param shape. (rh)
	void InitialiseParameterisedShape(vtkSmartPointer<vtkPolyData> shape);

	// Description:
		// Return the bsize parameters b that best model the given shape
		// (in standard deviations). 
		// That is that the given shape will be approximated by:
		//
		// shape ~ mean + b[0] * sqrt(eigenvalue[0]) * eigenvector[0]
		//              + b[1] * sqrt(eigenvalue[1]) * eigenvector[1]
		//         ...
		//              + b[bsize-1] * sqrt(eigenvalue[bsize-1]) * eigenvector[bsize-1]
		// The shape is first allocated with the mean, using rigid-body alignment if specified
		// 200521rh: only used in functions not called from FaceScreen 
	void GetShapeParameters(vtkPolyData* shape, vtkDoubleArray* b, int bsize, int rigid_body = true);

public:
	// for an unseen RESAMPLED surface, return the parameters that best model it
	void GetApproximateShapeParametersFromResampledSurface(vtkPolyData* shape,
		vtkDoubleArray* b, int rigid_body);

	// Retrieve how many modes are necessary to model the given proportion of the variation.
	// proportion should be between 0 and 1
	int GetModesRequiredFor(float proportion);


protected:

	// ----- the data that is saved and loaded -----
	//200516rh : comments with ??????????? indicate these values are not loaded from/saved to the model file

	// S is the number of modes currently being used (while computing is the number of examples)
	int S;  // 200516rh: ... loaded from the model file. Dimension 1 of evecMat2, e.g., 49

	// N is the number of vertices in the surface
	int N; // 200516rh: ... loaded from the model file. Dimension 2 of evecMat2, e.g., 23768

	// Eigenvalues (Sx1)
	vtkDoubleArray* Evals; // 200516rh: ... loaded from the model file.

	// the sum of the eigenvalues 
	// (NB. Evals is not necessarily complete, since we often only store 98%)
	float eigen_total; // 200516rh: ... loaded from the model file.

	// Matrix where each column is an eigenvector (3NxS)
	double** evecMat2; // 200516rh: ... loaded from the model file.

	// The mean shape in a vector (3Nx1)
	double* meanshape; // 200516rh: mean_shape is loaded from the model file. Dimension, e.g., [3, N], i.e., [3,23768]
	// the original mean landmarks (we align with these when resampling a new surface)
	float* mean_landmarks;	// 200516rh: mean_landmarks are loaded from the model file. Dimension, [3, n_landmarks], e.g., [3, 24]
	float* current_landmarks; // 200516rh: This seem to be the subject landmarks, snapped to the computed mean surface, ...
							  // .... having assigned a pseudo_landmark_index (see fct, vtkSurfacePCA::GetParameterisedLandmarks).

	vtkIdType* pseudo_landmark_indexes;  // ?????????? 200516rh: This seems to be arbitrarily made up, see fct. vtkSurfacePCA::GetParameterisedLandmarks(vtkPolyData* surface, vtkPolyData* landmarks)
	bool pseudo_landmarks_loaded; // ???????? 200516rh: true, if pseudo_landmark_indexes have been created in fct. GeTParameterisedLandmarks(..), false (set by ctor) otherwise 

	float* mean_surface_landmarks; // ?????????? 200516rh: This seems to be mean_landmarks associated to the mesh surface for the subject under investigation.
	// where the landmarks would be on the mean shape (3*n_landmarks x 1)
	int n_landmarks; // 200516rh: ... loaded from the model file. Number of mean_landmarks, e.g, 24.


	 //--- data that is used while computing the model -----//delclared publicc fo thread access

	vtkPolyData* example_surface; // ??????????  200516rh: requires example_landmarks (below). Only used by DSMBuilder.
	vtkPolyData* example_landmarks; // ??????????  200516rh: generated by fct. vtkSurfacePCA::SetExampleLandmarks, but this fct. is never called! Perhaps only used by DSMBuilder.
	vtkDoubleArray** modes_for_inputs; // (3NxS)  // ??????????  200516rh: computed in fct  vtkSurfacePCA::ComputeModesForInputs(int s) ...
									  //  ... with a call to vtkSurfacePCA::GetShapeParameters(this->GetInput(i),this->modes_for_inputs[i],s); 

	// the tcoords for the surface (2Nx1)
	double* tcoords; // 200516rh: ... loaded from the model file. Dimensions, e.g., 2 x 23768, all zero values.

	// the triangles for the surface (point-index triplets) (n_cells x 1)
	int n_cells; // 200516rh: ... loaded from the model file. Number of cells (3 uint values each), e.g., 46416.
	vtkIdType* polys; // 200516rh: ... loaded from the model file.


	vtkSurfacePCA();
	~vtkSurfacePCA();

private:
	// file IO functions
	bool SaveToFile(FILE* out, float proportion); // not required for heatmap computation
	bool LoadFromFile(FILE* in);

private:
	vtkSurfacePCA(const vtkSurfacePCA&);  // Not implemented.
	void operator=(const vtkSurfacePCA&);  // Not implemented.
};

#endif // __vtkSurfacePCA_h
