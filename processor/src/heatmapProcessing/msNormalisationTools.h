#ifndef MSNORMALISATIONTOOLS_H
#define MSNORMALISATIONTOOLS_H

#include <string>
#define CString std::string //TODO remove

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkPolyDataNormals.h>

#include "../mathUtils/C3dVector.h"
#include "vtkSurfacePCA.h"

using namespace std;

//rh TODO: address all these memory leaks: fields, mode_values, C3dVector etc.

class msNormalisationTools
{
public:
	bool LoadProjectionFile(string projection_FileName);

protected: 
	CString GetField(int r,int c);
	vtkSurfacePCA *pca;
	int GetColumnIndex(CString column_label);
	int N_EXAMPLES, N_CLASSIFICATIONS;
	
	
	vtkSmartPointer<vtkPolyData> *ref_surfaces;
	int *ref_example_indexes;
	//
	CString **fields;
	vtkDoubleArray **signatureScalars;
    vtkDoubleArray **mode_values;
	bool b_use_curvature;
	int N_ref_surfaces;
	CString current_ref_class;
	int GetNumTrainingModes() { return this->pca->GetTotalNumModes(); } //this->N_MODES; }
	void GetMeanModesForSet(int *set,int n_set, vtkDoubleArray *&mean_modes );

public:
	void SetPCAModel(vtkSurfacePCA *pca) { this->pca = pca; };
	msNormalisationTools(void);
	bool is_model_loaded;
	
	vtkSurfacePCA* GetModel() { return this->pca; }
	vtkDoubleArray* GetTrainingExampleModes(int i) { return this->mode_values[i]; }
	void SetNExamples(int n_examples) { this->N_EXAMPLES = n_examples; };
	void SetNClassifications(int n_classificaitons) { this->N_CLASSIFICATIONS = n_classificaitons; };
	
	// Error return codes:
	// -1 : from_var missing (usually set to "control")
	// -2 : N_refs < 2 (Insuficient reference surfaces)
	// -3 : Age column missing
	// -4 : Dx column (sometimes referred to as syndrome) missing 
	int CalculateMatchedMeanSignificance(vtkPolyData *surface, vtkSmartPointer<vtkDoubleArray> b, int age, 
		int mm_n = 35, // a.k.a. n_ma - moving average for comparison
		CString from_class = std::string("Dx"),  // a.k.a. mm_background_class 
		CString from_var = std::string("control"), // a.k.a. mm_background_var 
		C3dVector axes[3] = new C3dVector(0, 0, 0),
		int which_axis = -1, 
		bool write_to_file = false);

	void GenerateMatchedMeanForAge(std::vector<CString> mm_filter_classes, int mm_n, CString from_class, CString from_var, int age);
	void CalculateSignature(vtkPolyData *surface, vtkSmartPointer<vtkDoubleArray> b, int from_class, CString from_var, C3dVector axes[3],int which_axis, bool write_to_file);
	void CalculateSignature(vtkPolyData *surface, vtkSmartPointer<vtkDoubleArray> b, int *example_index_array, int ma, C3dVector axes[3],int which_axis, bool write_to_file, float scale_factor);
	void CalculateSignature(vtkPolyData *surface,vtkSmartPointer<vtkDoubleArray> b, C3dVector axes[3],int which_axis, bool write_to_file, float scale_factor);
 
	//Generate the surfaces for the reference class for showing statistical significance and modifies ref_example_indexes to show indexes of ref individuals returns number of examples in ref class
	int GetNRefSurfaces(int from_class, CString from_var, std::vector<CString> filter_classes, std::vector<CString> filter_values);
	void GenerateRefClassSurfaces(int from_class,CString from_var);
	void GenerateFilteredRefIndexes(int from_class, CString from_var,  std::vector<CString> filter_classes, std::vector<CString> filter_values,  int *&ref_indexes, int &N_refs);
	//void GenerateFilteredRefIndexes(int from_class, CString from_var,  std::vector<CString> filter_values, int *&ref_indexes, int &N_refs);
	void GenerateFilteredRefClassSurfaces(int from_class, CString from_var, std::vector<CString> filter_classes, std::vector<CString> filter_values);
	// Generates the reference class surfaces for significance only for a given set of example indexes,modifies ref_example_indexes to show indexes of ref individuals returns number of examples in ref class
	void GenerateRefClassSurfaces(int *example_index_array, int ma);
	//Calculate dist between two surfaces based on euclidean_option
	float CalculateDBetweenSurfaces(int i, int euclidean_option, vtkPolyDataNormals *surface_normals, double p_on_surface[3],double p_on_reference[3], C3dVector axes[3],int which_axis); 
	

	//GetColumnIndex

	~msNormalisationTools(void);
};

#endif // MSNORMALISATIONTOOLS_H
