#ifndef CLASSIFICATIONTOOLS_H
#define CLASSIFICATIONTOOLS_H

//#include <vtkAutoInit.h>
//VTK_MODULE_INIT(vtkRenderingOpenGL);
//VTK_MODULE_INIT(vtkInteractionStyle); 
//VTK_MODULE_INIT(vtkRenderingFreeType);
#include "vtkPolyData.h"
#include "vtkPointSet.h"
#include "vtkSmartPointer.h"
#include "vtkContextView.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkImageData.h"

#include <filesystem>
#include <string>

#define CString std::string //TODO: substitute - keep it now only for easier review of legacy code

class ClassificationTools
{
public:
	ClassificationTools();
	~ClassificationTools();

public:	 
 	// Compute closest mean classification of face mesh.
 	// Returns false if classification was not possible, e.g., due to error reading model files, true otherwise.
 	// Closest mean classification result is returned in float parameters mean and standardErr.
	 bool OnProjectIndividualsInSplitFolders(std::filesystem::path root_folder, const vtkSmartPointer<vtkPolyData> example_surface, 
	     vtkSmartPointer<vtkPolyData> example_landmarks, float &mean, float &standardErr);
	 void SetNSplits(int n) { this->N_SPLITS = n; };

	CString root_folder;	//root folder containing splits
	int N_SPLITS;			//number of splits default 20
	
  	//table/matrix containing projected mode values for each split [split][mode_number]	
  	//std::vector<std::vector<float>> projected_table; 
  	typedef std::vector<float> classificationValuesFromSplit;
  	typedef std::vector<classificationValuesFromSplit> splitClassificationValueMatrix;
  	splitClassificationValueMatrix projected_table;
  	
	//int n_modes;
	std::vector<float> CMValues;
	void DrawGraph(float value, float standardErr, CString individual, CString thumbnailImage, bool offScreenRender, vtkSmartPointer<vtkImageData> &imageData);

	int GetNSplits() { return this->N_SPLITS; };
	bool OnClassifyIndividualsUsingClosestMean(std::filesystem::path root_folder, int split);
	// rh: Function unused:
	void ProjectIndividualInSplit(CString model_filename, vtkSmartPointer<vtkPolyData> surface, vtkSmartPointer<vtkPolyData> landmarks, int split_num);
	bool ProjectResampledIndividualInSplit(std::filesystem::path root_folder, std::filesystem::path model_filename, vtkSmartPointer<vtkPolyData> surface, int split_num);

protected:	
	vtkContextView *contextView; // Most likely going to be unused: Generating 2D graphic.
};

struct ValueClass { 
    float value,which_class; 
    ValueClass(float v,float wc):value(v),which_class(wc) {}
    ValueClass(const ValueClass& vc):value(vc.value),which_class(vc.which_class) {}
    const ValueClass& operator=(const ValueClass& vc) 
	{ 
        value=vc.value;
        which_class=vc.which_class;
        return *this;
    }
	bool operator<(const ValueClass& right) 
	{ 
		return value<right.value; 
	}
};

typedef std::vector<ValueClass> ValueClassList;

typedef struct 
{ 
	char *model_name;
	int split_num;
	ClassificationTools *pThis;
	vtkSmartPointer<vtkPolyData> example_surface;  
	vtkSmartPointer<vtkPolyData> example_landmarks;
} 
SplitParams, *PSplitParams;

#endif // CLASSIFICATIONTOOLS_H
