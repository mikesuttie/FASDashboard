#include <string>
#define CString std::string //TODO remove

#include "msNormalisationTools.h"

#include "../mathUtils/C3dVector.h" 

//#include <vtkAutoInit.h>
//VTK_MODULE_INIT(vtkRenderingOpenGL);
//VTK_MODULE_INIT(vtkInteractionStyle); 
//VTK_MODULE_INIT(vtkRenderingFreeType);
#include <vtkMath.h>
#include <vtkPointData.h>

#define mfcGUImessage(X) std::cout << "In msNormalisationTools.cpp: GUI message: " << X << endl;

using namespace std;

// Free function for std::string formatting a la printf, replacing call to .Format method on MFC CStrings
// May become obsolete when 'fields' datastructure is replaced.
// Also used in classificationTools.cpp
#include <memory>
#include <stdexcept>
template<typename ... Args>
std::string string_format(const std::string& format, Args ... args)
{
	size_t size = static_cast<size_t>(snprintf(nullptr, 0, format.c_str(), args ...)) + 1; // Extra space for '\0'
	if (size <= 0) { throw std::runtime_error("Error during formatting."); }
	std::unique_ptr<char[]> buf(new char[size]);
	snprintf(buf.get(), size, format.c_str(), args ...);
	return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

msNormalisationTools::msNormalisationTools(void)
{
	this->pca = NULL;
    this->is_model_loaded = false;
	this->fields = NULL;
    this->mode_values = NULL;
	this->b_use_curvature = false;
 
    this->N_CLASSIFICATIONS = 0;
    this->N_EXAMPLES = 0;
	this->signatureScalars = NULL;

	this->current_ref_class ="";
	this->ref_surfaces = NULL;
}

msNormalisationTools::~msNormalisationTools(void)
{
//	this->pca->Delete(); 
//	this->pca=NULL; 

	if(this->ref_surfaces!=NULL)
		delete []this->ref_surfaces;
 
    if(this->mode_values)
    {
        for(int i=0;i<this->N_EXAMPLES;i++)
            this->mode_values[i]->Delete();;
        delete []this->mode_values;
        this->mode_values = NULL;
    }
	if (fields)
	{
		delete[] fields;
	}
}

//Sort by double array values
void SortSet(int *&index_array, double *&double_array, int n)
{
	double x;
	int e;
	//Little sorting algorithm		 //TODO rh 200515: is doesn't look correct, prefer std:sort with suitable lambda
	for(int a = 1; a < n; a++ ) {
		for(int b = 0; b < n - 1; b++ ) {
			if( double_array[b] > double_array[b + 1] ) {
				x = double_array[ b ];
				e = index_array[b];
				double_array[b] = double_array[b + 1];
				double_array[b + 1] = x;
				index_array[b] = index_array[b + 1];
				index_array[b + 1] = e;
				}
			}
		} 
}

void ScaleSurfacePoints(double scale_factor, vtkPolyData *surface)
{
	//apply the scale factor to surface
	vtkPoints *points = vtkPoints::New();
	const int N_POINTS = surface->GetNumberOfPoints();
	for(int i = 0; i<N_POINTS; i++) {	
		double *pA = surface->GetPoint(i);
		pA[0]=pA[0]*scale_factor;
		pA[1]=pA[1]*scale_factor;
		pA[2]=pA[2]*scale_factor;
		points->InsertPoint(i,pA);
	}
		 
	surface->SetPoints(points);
}

CString msNormalisationTools::GetField(int r,int c)
{
    if(r>=0 && r<N_EXAMPLES+1 && c>=0 && c<N_CLASSIFICATIONS+1)
        return this->fields[r][c];
    else
        return "";
}

int msNormalisationTools::GetColumnIndex(CString column_label)
{
	auto stringsAreEqual_CaseInsensitive = [](const CString& str1, const CString& str2) -> bool
	{
		return std::equal(str1.begin(), str1.end(), str2.begin(),
			[](char c1, char c2) -> bool
			{ return std::toupper(c1) == std::toupper(c2); }
		);
	};

	// find the index of the column with this classification in it
	for(int i=0;i<this->N_CLASSIFICATIONS+1;i++)
		//if(column_label.CompareNoCase(GetField(0,i))==0)  -->
		if (stringsAreEqual_CaseInsensitive(column_label, GetField(0, i)))
		{
			return i;
		}
	//else failed to find
	mfcGUImessage("Could not find column (in fct GetColumnIndex(CString column_label) ) "); 
	return -1; // didn't find!
}

int msNormalisationTools::CalculateMatchedMeanSignificance(vtkPolyData *surface, vtkSmartPointer<vtkDoubleArray> b, int age, int mm_n, CString from_class, CString from_var, C3dVector axes[3],int which_axis, bool write_to_file)
{
	std::vector<CString> example_filter_values;
	std::vector<CString> example_filter_classes;

	if(from_var.compare("") == 0)
	{
		return -1;
	}

	const int idx_from_class_Column = GetColumnIndex(from_class);
	if(idx_from_class_Column == -1)
	{
		return -4;
	}

	//Generate our reference surface ... using filter
	int N_refs(0);	 
	this->GenerateFilteredRefIndexes(idx_from_class_Column, from_var, example_filter_classes, example_filter_values, this->ref_example_indexes , N_refs);	 
	if(N_refs<2)	
	{ 		
		return -2; // mfcGUImessage("Not enough reference surfaces, check matched mean options"); 
	} 

	const int idxAgeColumn = this->GetColumnIndex("age"); 
	if(idxAgeColumn == -1)
	{
		return -3;
	}

	float mean_age(0.0F); 
	//now we need to match the given reference surfaces by age and using mm_N reference examples
	//uses much the same code as moving avergae in rapid phenotyping 
	CString filters = "";
		for(int i = 0; i<example_filter_values.size(); i++)
			filters+="-"+example_filter_values.at(i); //cstring list of filter vars used
	// in the format "MM-AGE-VAR-FILTER1-FILTER2-FILTER3";

	if(mm_n>=N_refs) //use them all IF USER param exceeds that of the n_ref surfaces
	{
		//Calculate mean surface, add it to the current_matched_mean_mode_values  
		this->GetMeanModesForSet(this->ref_example_indexes, N_refs, this->mode_values[this->N_EXAMPLES-1]);
		for(int i = 0; i<N_refs; i++)
			mean_age += std::stof(this->GetField(this->ref_example_indexes[i]+1, idxAgeColumn));
		mean_age = mean_age/(float)N_refs;//need to ROUND THIS!
		mean_age = floor(mean_age * 10 + 0.5)/10 ; //round to 1 dp
		
		CString matched_mean_label;
		matched_mean_label = string_format("MEAN-%.1f-%s-%s n=%d", mean_age, from_var, filters, N_refs); //MFC: matched_mean_label.Format("MEAN-%.1f-%s-%s n=%d", mean_age, from_var, filters, N_refs);
		//generate the surfacs
		//this->GenerateRefClassSurfaces(this->ref_example_indexes,this->N_ref_surfaces);
		this->fields[this->N_EXAMPLES][0] =  matched_mean_label;
		this->fields[this->N_EXAMPLES][idxAgeColumn] = string_format("%f", mean_age); //MFC: this->fields[this->N_EXAMPLES][idxAgeColumn].Format("%f", mean_age);
		for(int i =0; i < this->GetNumTrainingModes(); i++)//this is a long way of saying write the mode values into the fields table
			this->fields[this->N_EXAMPLES][this->N_CLASSIFICATIONS - this->GetNumTrainingModes() + 1 + i] = string_format("%f", this->mode_values[this->N_EXAMPLES - 1]->GetValue(i)); 
				//MFC: this->fields[this->N_EXAMPLES][this->N_CLASSIFICATIONS-this->GetNumTrainingModes()+1+i].Format("%f",this->mode_values[this->N_EXAMPLES-1]->GetValue(i));
				//return CalculateSignature(example_index, axes, which_axis, write_to_file, 1);
		CalculateSignature(surface, b, this->ref_example_indexes, N_refs, axes, which_axis, write_to_file, 1);
		return 0;
	}

	int n_moving_averages = N_refs-mm_n+1;
	double *moving_average_ages= new double[n_moving_averages];
	//else grab the correct N of examples and the best matched age
	double *control_age_array = new double[N_refs]; // array of ages for the control set
	//int *background_examples_index_array = new int[mm_n]; // resulting background exaamples for this target example
	for(int i=0;i<N_refs;i++)	 // get array of ages
		control_age_array[i] = std::stof(this->GetField( this->ref_example_indexes[i]+1,idxAgeColumn )); 
	//Sort the set so we have a sorted list of index's for matched mean calculation
	SortSet(this->ref_example_indexes, control_age_array, N_refs );
	// Calculate running mean ages 
	for(int i = 0; i < n_moving_averages; i++)
	{
		mean_age = 0;
		for(int m = 0; m<mm_n; m++)
			mean_age+=control_age_array[i+m];
		moving_average_ages[i] = mean_age/(double)mm_n;
	}
	delete[] control_age_array;

	int *matched_mean_set = new int[mm_n];
	float target_age = age;
	int ma_index = 0;
	//Find the closest moving average age and add appropriate to the moving average set
	while(target_age > moving_average_ages[ma_index] && ma_index<n_moving_averages-1)
		ma_index++;
	//if not the first in the list then find the closest
	if(ma_index != 0)
	{
		double ma1 = target_age-moving_average_ages[ma_index-1];
		double ma2 = moving_average_ages[ma_index]-target_age;
		if(ma1<ma2)
			ma_index--; // change index to reflect the choice
		//index remains the same otherwise
	}
	mean_age = moving_average_ages[ma_index];
	//  We now have the target individual data and the age of it age matched mean
	//	create a set of moving averages by accessing the sorted control set
	//	works out as indexes from ma_index to ma_index+i_ma in sorted list
	for(int index = 0; index<mm_n; index++)
	{
		matched_mean_set[index] = this->ref_example_indexes[ma_index];
		ma_index++;
	}
	//clean up
	delete []moving_average_ages;
	//Calculate mean surface, add it to the current_matched_mean_mode_values  
	{
		mean_age = floor(mean_age * 10 + 0.5)/10 ;
		//Grab the mea modes for the set and assign to the mode values
		GetMeanModesForSet(matched_mean_set, mm_n, this->mode_values[this->N_EXAMPLES-1]);
		CString matched_mean_label;
		matched_mean_label = string_format("MEAN-%.1f-%s-%s n=%d", mean_age, from_var, filters, mm_n); //MFC: matched_mean_label.Format("MEAN-%.1f-%s-%s n=%d", mean_age, from_var, filters, mm_n);
		this->fields[this->N_EXAMPLES][0] =  matched_mean_label;
		this->fields[this->N_EXAMPLES][idxAgeColumn] = string_format("%f", mean_age); //MFC: this->fields[this->N_EXAMPLES][idxAgeColumn].Format("%f", mean_age);
		for(int i =0; i < this->GetNumTrainingModes(); i++)//this is a long way of saying write the mode values into the fields table
			this->fields[this->N_EXAMPLES][this->N_CLASSIFICATIONS - this->GetNumTrainingModes() + 1 + i] = string_format("%f", this->mode_values[this->N_EXAMPLES - 1]->GetValue(i)); //MFC: this->fields[this->N_EXAMPLES][this->N_CLASSIFICATIONS-this->GetNumTrainingModes()+1+i].Format("%f",this->mode_values[this->N_EXAMPLES-1]->GetValue(i));
	}
	 
	//return significance
	CalculateSignature(surface, b, matched_mean_set, mm_n, axes, which_axis, write_to_file, 1);
	return 0;
 }
  
void msNormalisationTools::GenerateRefClassSurfaces(int from_class, CString from_var)
{
	if(this->current_ref_class.compare(from_var)==0)
		return;//Don't bother
  
	//CWaitCursor waiting;
	
	if(this->ref_surfaces!=NULL)  //delete original
		delete []this->ref_surfaces;
  
	current_ref_class = from_var;
	//Find out how many examples there are in the reference class and obtan surfaces for them ( as we will need this!) 
	// count number of ref surfaces
	this->N_ref_surfaces = 0;
	for(int example = 0; example<this->N_EXAMPLES; example++)
		if(this->GetField(example+1,from_class).compare(from_var)==0)
			this->N_ref_surfaces++;
	
	int *ref = new int[this->N_ref_surfaces];
	this->ref_surfaces = new vtkSmartPointer<vtkPolyData>[this->N_ref_surfaces]; 
	
	int index=0;
	// Create array of vtkpolydata for the reference surfaces in matching group
	for(int example = 0; example<this->N_EXAMPLES; example++)
		if(this->GetField(example+1,from_class).compare(from_var)==0) //Select only from the reference class
		{
			this->ref_surfaces[index] = vtkSmartPointer<vtkPolyData>::New();				
			this->pca->GetParameterisedShape(this->mode_values[example],this->ref_surfaces[index]);	
			 
			ref[index] = example;
			index++;
		}
	this->ref_example_indexes = ref;
}

void msNormalisationTools::GenerateFilteredRefIndexes(int from_class, CString from_var, std::vector<CString> filter_classes, std::vector<CString> filter_values,  int *&ref_indexes, int &N_refs)
{
	//CWaitCursor waiting;
	CString current_ref_class = from_var;
	//Find out how many examples there are in the reference class and obtan surfaces for them ( as we will need this!) 
	// count number of ref surfaces
	N_refs = 0;
	for(int example = 0; example<this->N_EXAMPLES; example++)
		if(this->GetField(example+1,from_class).compare(from_var)==0)
		{
			bool discard = false; // dont discard it unles it fails to comply with filter
			for(int i = 0; i<filter_classes.size(); i++) // iterate through each filter classification
				if(this->GetField(example+1, this->GetColumnIndex(filter_classes.at(i))).compare(filter_values.at(i))!=0)// then it's fail the filters requirements
					discard = true;
			if(!discard) // only count if we're not discarding
				N_refs++;
		}
	
	if(ref_indexes==NULL)
		delete[] ref_indexes;

	ref_indexes = new int[N_refs];
	int index=0;
	// Create array of vtkpolydata for the reference surfaces
	for(int example = 0; example<this->N_EXAMPLES; example++)
		if(this->GetField(example+1,from_class).compare(from_var)==0)
		{
			bool discard = false; // dont discard it unles it fails to comply with filter
			for(int i = 0; i<filter_classes.size(); i++) // iterate through each filter classification
				if(this->GetField(example+1, this->GetColumnIndex(filter_classes.at(i))).compare(filter_values.at(i))!=0)// then it's fail the filters requirements
					discard = true;
			if(!discard) // only count if we're not discarding
			{	
				ref_indexes[index] = example;
				index++;
			}
		}
}

int msNormalisationTools::GetNRefSurfaces(int from_class, CString from_var, std::vector<CString> filter_classes, std::vector<CString> filter_values)
{
	int N = 0;
	for(int example = 0; example<this->N_EXAMPLES; example++)
		if(this->GetField(example+1,from_class).compare(from_var)==0)
		{
			bool discard = false; // dont discard it unles it fails to comply with filter
			for(int i = 0; i<filter_classes.size(); i++) // iterate through each filter classification
				if(this->GetField(example+1, this->GetColumnIndex(filter_classes.at(i))).compare(filter_values.at(i))!=0)// then it's fail the filters requirements
					discard = true;
			if(!discard) // only count if we're not discarding
				N++;
		}
		return N;
}

void msNormalisationTools::GenerateFilteredRefClassSurfaces(int from_class, CString from_var, std::vector<CString> filter_classes, std::vector<CString> filter_values)
{
	//CWaitCursor waiting;
	
	if(this->ref_surfaces!=NULL)  //delete original
		delete []this->ref_surfaces;
	 
	current_ref_class = from_var;
	//Find out how many examples there are in the reference class and obtan surfaces for them ( as we will need this!) 
	// count number of ref surfaces
	this->N_ref_surfaces = 0;
	for(int example = 0; example<this->N_EXAMPLES; example++)
		if(this->GetField(example+1,from_class).compare(from_var)==0)
		{
			bool discard = false; // dont discard it unles it fails to comply with filter
			for(int i = 0; i<filter_classes.size(); i++) // iterate through each filter classification
				if(this->GetField(example+1, this->GetColumnIndex(filter_classes.at(i))).compare(filter_values.at(i))!=0)// then it's fail the filters requirements
					discard = true;
			if(!discard) // only count if we're not discarding
				this->N_ref_surfaces++;
		}
 
	int *ref = new int[this->N_ref_surfaces];
	this->ref_surfaces = new vtkSmartPointer<vtkPolyData>[this->N_ref_surfaces];
  	
	int index=0;
	// Create array of vtkpolydata for the reference surfaces
	for(int example = 0; example<this->N_EXAMPLES; example++)
		if(this->GetField(example+1,from_class).compare(from_var)==0)
		{
 			bool discard = false; // dont discard it unles it fails to comply with filter
			for(int i = 0; i<filter_classes.size(); i++) // iterate through each filter classification
				if(this->GetField(example+1, this->GetColumnIndex(filter_classes.at(i))).compare(filter_values.at(i))!=0)// then it's fail the filters requirements
					discard = true;
			if(!discard) // only count if we're not discarding
			{	
				this->ref_surfaces[index] = vtkSmartPointer<vtkPolyData>::New();	
 
				this->pca->GetParameterisedShape(this->mode_values[example],this->ref_surfaces[index]);	
 
				ref[index] = example;
				index++;
			}
		}
	this->ref_example_indexes = ref;
}
			
 

void msNormalisationTools::GenerateRefClassSurfaces(int *ref_class_index_array, int n_ma) 
{	
	//CWaitCursor waiting;
  
	if(this->ref_surfaces!=NULL)  //delete original
		delete []this->ref_surfaces;
  
	this->N_ref_surfaces = n_ma;
	this->ref_surfaces = new vtkSmartPointer<vtkPolyData>[this->N_ref_surfaces];
	 
 	// Create array of vtkpolydata for the reference surfaces
	#pragma omp parallel for
	for(int example = 0; example<n_ma; example++)
	{
			this->ref_surfaces[example] = vtkSmartPointer<vtkPolyData>::New();	
			int index = ref_class_index_array[example];
			this->pca->GetParameterisedShape(this->mode_values[index],this->ref_surfaces[example]);	
	}
	this->ref_example_indexes = ref_class_index_array;
}
   
void msNormalisationTools::GetMeanModesForSet(int *set,int n_set, vtkDoubleArray *&mean_modes)
{	//calcualte mean modes for a given set, save to mean_modes
	for(int i = 0; i < this->GetNumTrainingModes(); i++)
	{
		double c = 0;	
		for(int example = 0; example<n_set; example++)
			c+=this->GetTrainingExampleModes(set[example])->GetValue(i); //Cumulate mode for each example
		//Divide cumulative total by n, add to mode value array
		mean_modes->SetValue(i,(c/n_set));			 
	} 
}

void msNormalisationTools::GenerateMatchedMeanForAge(std::vector<CString> mm_filter_classes, int mm_n, CString from_class, CString from_var, int age)
{
	std::vector<CString> example_filter_values;/*

	for(int i = 0; i < mm_filter_classes.size(); i++)
	{//get the col index for the filter classification 
		int col_index = this->GetColumnIndex(mm_filter_classes.at(i));
		//grab the clasification value for this example from the fields table and add to the vector
		example_filter_values.push_back(this->GetField(example_index+1, col_index));
	}//we now have two vectors to use 1 comtaining the column labels for the filters and one containing the correshtponding variables for that exampe*/

	// Just in case
	//ASSERT(from_class!=-1); (200515rh: type mismatch)
	if(from_var=="")	return;//Generate our reference surface
	//CWaitCursor waiting;
	//now need to grab age column
	int iAgeColumn = this->GetColumnIndex("age"); // should find it 
	if(iAgeColumn==-1)		return;//failed, user will already have seen error message from GetColumnIndex
	float mean_age ; 
	//now we nned to match the reference surfaces by age and using mm_N reference examples
	//uses much the same code as moving avergae in rapid phenotyping 
	int *ref_indexes;
	int N_refs;
	this->GenerateFilteredRefIndexes(this->GetColumnIndex(from_class), from_var, mm_filter_classes, example_filter_values, ref_indexes, N_refs);
	
	CString filters = "";
		for(int i = 0; i<example_filter_values.size(); i++)
			filters+="-"+example_filter_values.at(i); //cstring list of filter vars used
	// in the format "MM-AGE-VAR-FILTER1-FILTER2-FILTER3";

	if(mm_n>=N_refs) //use them all IF USER param exceeds that of the n_ref surfaces
	{
		mean_age =0;
		//Calculate mean surface, add it to the current_matched_mean_mode_values  
		GetMeanModesForSet(ref_indexes, N_refs, this->mode_values[this->N_EXAMPLES-1]);
		for(int i = 0; i<N_refs; i++)
			mean_age += std::stof(this->GetField(ref_indexes[i]+1, iAgeColumn));
		mean_age = mean_age/(float)N_refs;//need to ROUND THIS!
		mean_age = floor(mean_age * 10 + 0.5)/10 ;
		
		CString matched_mean_label;
		matched_mean_label = string_format("MEAN-%.1f-%s-%s n=%d", mean_age, from_var, filters, N_refs);  //MFC: matched_mean_label.Format("MEAN-%.1f-%s-%s n=%d", mean_age, from_var, filters, N_refs);
	 
		this->fields[this->N_EXAMPLES][0] =  matched_mean_label;
		this->fields[this->N_EXAMPLES][iAgeColumn] = string_format("%f", mean_age); //MFC: this->fields[this->N_EXAMPLES][iAgeColumn].Format("%f", mean_age);
		for(int i =0; i < this->GetNumTrainingModes(); i++)//this is a long way of saying write the mode values into the fields table
			this->fields[this->N_EXAMPLES][this->N_CLASSIFICATIONS - this->GetNumTrainingModes() + 1 + i] = string_format("%f", this->mode_values[this->N_EXAMPLES - 1]->GetValue(i)); //MFC: this->fields[this->N_EXAMPLES][this->N_CLASSIFICATIONS-this->GetNumTrainingModes()+1+i].Format("%f",this->mode_values[this->N_EXAMPLES-1]->GetValue(i));
		return;
	}

	int n_moving_averages = N_refs-mm_n+1;
	double *moving_average_ages= new double[n_moving_averages];
	//else grab the correct N of examples and the best matched age
	double *control_age_array = new double[N_refs]; // array of ages for the control set
	//int *background_examples_index_array = new int[mm_n]; // resulting background exaamples for this target example
	for(int i=0;i<N_refs;i++)	 // get array of ages
		control_age_array[i] = std::stof(this->GetField( ref_indexes[i]+1,iAgeColumn )); 
	//Sort the set so we have a sorted list of index's for matched mean calculation
	SortSet(ref_indexes, control_age_array, N_refs);
	// Calculate running mean ages 
	for(int i = 0; i < n_moving_averages; i++)
	{
		mean_age = 0;
		for(int m = 0; m<mm_n; m++)
			mean_age+=control_age_array[i+m];
		moving_average_ages[i] = mean_age/(double)mm_n;
	}
	delete[] control_age_array;

	int *matched_mean_set = new int[mm_n];
	float target_age = age;
	int ma_index = 0;
	//Find the closest moving average age and add appropriate to the moving average set
	while(target_age > moving_average_ages[ma_index] && ma_index<n_moving_averages-1)
		ma_index++;
	//if not the first in the list then find the closest
	if(ma_index != 0)
	{
		double ma1 = target_age-moving_average_ages[ma_index-1];
		double ma2 = moving_average_ages[ma_index]-target_age;
		if(ma1<ma2)
			ma_index--; // change index to reflect the choice
		//index remains the same otherwise
	}
	mean_age = moving_average_ages[ma_index];
	//  We now have the target individual data and the age of it age matched mean
	//	create a set of moving averages by accessing the sorted control set
	//	works out as indexes from ma_index to ma_index+i_ma in sorted list
	for(int index = 0; index<mm_n; index++)
	{
		matched_mean_set[index] = ref_indexes[ma_index];
		ma_index++;
	}
	//clean up
	delete []moving_average_ages;
	//Calculate mean surface, add it to the current_matched_mean_mode_values  
	{
		mean_age = floor(mean_age * 10 + 0.5)/10 ;
		//Grab the mea modes for the set and assign to the mode values
		GetMeanModesForSet(matched_mean_set, mm_n, this->mode_values[this->N_EXAMPLES-1]);
		CString matched_mean_label;
		matched_mean_label = string_format("MEAN-%.1f-%s-%s n=%d", mean_age, from_var, filters, mm_n); //MFC: matched_mean_label.Format("MEAN-%.1f-%s-%s n=%d", mean_age, from_var, filters, mm_n);
		this->fields[this->N_EXAMPLES][0] =  matched_mean_label;
		this->fields[this->N_EXAMPLES][iAgeColumn] = string_format("%f", mean_age); //MFC: this->fields[this->N_EXAMPLES][iAgeColumn].Format("%f", mean_age);
		for(int i =0; i < this->GetNumTrainingModes(); i++)//this is a long way of saying write the mode values into the fields table
			this->fields[this->N_EXAMPLES][this->N_CLASSIFICATIONS-this->GetNumTrainingModes()+1+i] = string_format("%f",this->mode_values[this->N_EXAMPLES-1]->GetValue(i)); //MFC: this->fields[this->N_EXAMPLES][this->N_CLASSIFICATIONS-this->GetNumTrainingModes()+1+i].Format("%f",this->mode_values[this->N_EXAMPLES-1]->GetValue(i));
	}
	delete []ref_indexes;
 }
 
  
void msNormalisationTools::CalculateSignature(vtkPolyData *surface, vtkSmartPointer<vtkDoubleArray> b, int from_class, CString from_var, C3dVector axes[3],int which_axis, bool write_to_file)
{
	// Just in case
	// ASSERT(from_class!=-1); (200515rh: type mismatch)
	if(from_var=="")
		return;
	//
	//CWaitCursor waiting;
	if(!this->ref_surfaces)
		this->GenerateRefClassSurfaces(from_class, from_var);

	return CalculateSignature(surface, b, axes, which_axis, write_to_file,1);
}

void msNormalisationTools::CalculateSignature(vtkPolyData *surface, vtkSmartPointer<vtkDoubleArray> b, int *ref_class_index_array, int ma, C3dVector axes[3],int which_axis, bool write_to_file, float scale_factor)
{
	if(ref_class_index_array==NULL)
		return;

	//CWaitCursor waiting;
	int *r = ref_class_index_array;
 /*
	for(int i =0; i<ma; i++)
	{
		r = new int[ma];
		int index = 0;
		for(int j =0; j<ma; j++)
		{
			r[index] = ref_class_index_array[j];
			index++;
		}
		ma--; //reduce ma by 1
		break;
	}*/

	//generate reference class surfaces with given indexes
	this->GenerateRefClassSurfaces(r, ma);
	//delete ref_class_index_array 
	this->CalculateSignature(surface, b, axes, which_axis, write_to_file, scale_factor);
}

void msNormalisationTools::CalculateSignature(vtkPolyData *surface,  vtkSmartPointer<vtkDoubleArray> b, C3dVector axes[3],int which_axis, bool write_to_file, float scale_factor)
{
	if(!this->ref_surfaces)
		return; // there should be surfaces!
	//Obtain PCA modes
	//Create the required mean surface
	vtkPolyData *mean_surface = vtkPolyData::New();
  
	const int N = this->N_ref_surfaces;
	//Set n to the size of ref_example_index as this determines the number of example for the reference class	const int N = N_ref_surfaces;
	vtkDoubleArray *mean_mode_values = vtkDoubleArray::New();
	mean_mode_values->SetNumberOfComponents(1);
	mean_mode_values->SetNumberOfValues(this->GetNumTrainingModes());
	//Calculate mean surface  
	GetMeanModesForSet(this->ref_example_indexes, N, mean_mode_values);
	//Get mean and individual surface from mode values
	#pragma omp parallel sections
	{
		#pragma omp section
			this->pca->GetParameterisedShape(mean_mode_values, mean_surface);
	}

	//Mean and sd mode values no longer needed dump them	
	mean_mode_values->Delete(); 
	//Get num of points
	const int N_POINTS = mean_surface->GetNumberOfPoints();
	const int euclidean_option = 2;
	//TO CORRECT A  PIPELINE BUG
	vtkSmartPointer<vtkPolyData> temp_mean = 
		vtkSmartPointer<vtkPolyData>::New();
	temp_mean->DeepCopy(mean_surface);

	//Generate surface normals for mean ref surface
	vtkPolyDataNormals *surface_normals = vtkPolyDataNormals::New();
	surface_normals->SetInputData(temp_mean);
	surface_normals->Update(); // since we will bypass the pipeline soon
	
	//Result stdv scalar array/curvature values
	vtkDoubleArray *scalars = vtkDoubleArray::New();
	scalars->SetNumberOfComponents(1);
    scalars->SetNumberOfValues(N_POINTS);
	scalars->SetName("Stdv");
  
	float min_found=FLT_MAX,max_found=-FLT_MAX;
 
	//Calculate scalar values for mean d

	for(int i = 0; i<N_POINTS; i++)
	{
		float d=0;
		float sum_squares = 0;
		#pragma omp parallel for reduction(+:d) reduction(+:sum_squares)
		for(int example = 0; example<N; example++)
		{
			float dist(0.0F);
				if(!this->b_use_curvature)
					dist = this->CalculateDBetweenSurfaces(i, euclidean_option, surface_normals, mean_surface->GetPoint(i), ref_surfaces[example]->GetPoint(i), axes, which_axis);	 
	 
			d+=dist; 
			sum_squares += pow(dist,2);
		}
		
		//divide d by n to get the mean d for this point
		float sd_d = sqrt(sum_squares/(N-1));
 		float mean_d = d/N;
	 
		//Calculate surface d for this point
		float surface_d ;
		if(!this->b_use_curvature)
			surface_d = CalculateDBetweenSurfaces(i, euclidean_option, surface_normals, mean_surface->GetPoint(i), surface->GetPoint(i),axes, which_axis);
		else 
			surface_d = surface->GetPointData()->GetScalars()->GetTuple(i)[0] - mean_surface->GetPointData()->GetScalars()->GetTuple(i)[0];
		//Calcualte statistical significance dist
		float dist = 0;
		if(sd_d!= 0) //check we're not dividing by 0! Can happen if curvature at a point on example is equal to that of the mean. Dist then  =0 ;
		 dist = (surface_d-mean_d)/sd_d;
 
		if(dist<min_found)
			min_found=dist;
		if(dist>max_found)
			max_found=dist;	
		// assign the scalar at this vertex
		scalars->SetValue(i,dist); 
	}
	mean_surface->Delete();
	surface_normals->Delete();
	surface->GetPointData()->SetScalars(scalars);
	
    scalars->Delete();
}

float msNormalisationTools::CalculateDBetweenSurfaces(int i, int euclidean_option, vtkPolyDataNormals *surface_normals, double p_on_surface[3],double p_on_reference[3],C3dVector axes[3], int which_axis) 
{
 	// the straight point-to-point distance
	float dist = sqrt(vtkMath::Distance2BetweenPoints(p_on_reference,
			p_on_surface));

	// multiply the distance by the sign of the dot product between the difference
	// vector and the normal vector
	double surface_normal[3],difference[3];
	difference[0] = p_on_surface[0] - p_on_reference[0];
	difference[1] = p_on_surface[1] - p_on_reference[1];
	difference[2] = p_on_surface[2] - p_on_reference[2];
	
	surface_normal[0] = surface_normals->GetOutput()->GetPointData()->GetNormals()->GetComponent(i,0);
	surface_normal[1] = surface_normals->GetOutput()->GetPointData()->GetNormals()->GetComponent(i,1);
	surface_normal[2] = surface_normals->GetOutput()->GetPointData()->GetNormals()->GetComponent(i,2); 
	 
	if(which_axis < 3 && which_axis != -1)
	{
		C3dVector p1(p_on_reference[0], p_on_reference[1],p_on_reference[2]);
		C3dVector p2(p_on_surface[0], p_on_surface[1],p_on_surface[2]);
		// use the distance along an axis (which_axis)
		return DotProduct(p2-p1,axes[which_axis]);	 	
	}
	if(euclidean_option==1)
	{
		// multiply the distance by the sign of the dot product of the difference vector with the surface normal
		if(vtkMath::Dot(difference,surface_normal)<0.0)
				dist *= -1.0f;
	}
	else if(euclidean_option==2)
	{
		// multiply the distance by the dot product of the (normalized) difference with the surface normal at that point
		// this seems to give a very nice measure of the difference between two surfaces, as long as the normals
		// can be relied upon - where the surfaces have slid across each other the difference is zero
		vtkMath::Normalize(difference);
		vtkMath::Normalize(surface_normal);
		dist *= vtkMath::Dot(difference,surface_normal);
	}
	/*if(which_axis < 3 && which_axis != -1)
	{
		C3dVector p1(p_on_reference[0],p_on_reference[1],p_on_reference[2]);
		C3dVector p2(p_on_surface[0],p_on_surface[1],p_on_surface[2]);
		// use the distance along an axis (which_axis)
		dist = DotProduct(p2-p1,axes[which_axis]);	 	
	}*/
	return dist;
}

bool msNormalisationTools::LoadProjectionFile(string projection_FileName)
{
	// formerly CFaceMarkDoc::OpenAugmentedDatasetFile(CString filename)
	// TODO Insert checks for correct file format.
	auto openProjectionFile = [=](FILE* in) -> bool
	{
		if (in)
		{
			int i, j;
			char header[150];

			// how many rows and columns are there? 
			//MS excel comma fix, line parsing changed to sscanf from fscanf 
			fgets(header, 150, in);
			sscanf(header, "%d,%d", &N_EXAMPLES, &N_CLASSIFICATIONS);

			// size the fields array to (N_EXAMPLES+1)x(N_CLASSIFICATIONS+1)
			N_EXAMPLES++; // add an extra space for the matched mean
			
			if (fields)
			{
				delete[] fields;
			}
			fields = new CString * [N_EXAMPLES + 1];

			for (i = 0; i < N_EXAMPLES + 1; i++)
			{
				fields[i] = new CString[N_CLASSIFICATIONS + 1];
			}
			// read the fields
			CString text;
			char character;
			//  for(i=0;i<this->N_EXAMPLES+1;i++)
			for (i = 0; i < N_EXAMPLES; i++) // changed to allow for matched mean addition to the fields
			{
				for (j = 0; j < N_CLASSIFICATIONS + 1; j++)
				{
					// read the field up until either a comma or a newline or EOF
					text = "";
					do {
						character = fgetc(in);
						if (character != '\n' && character != ',' && character != EOF)
							text.append(1u, character); // append (blech!)
					} while (character != '\n' && character != ',' && character != EOF);
					// insert the text into the right field
					fields[i][j] = text;
				}
			}
			fclose(in);

			//0s for everything else
			for (int i = 1; i < N_CLASSIFICATIONS + 1; i++) fields[N_EXAMPLES][i] = "0";

			return true;
		}
		else //(!in)
		{
			cerr << "In FaceScreeningObject::openProjectionFile (msNormalisationTools.cpp): Could not open file for reading!" << endl;
			return false;
		}
	};

#define _CRT_SECURE_NO_WARNINGS // Disable deprecation warning (for fopen)
	FILE* projectionFile = fopen(projection_FileName.c_str(), "rt");
	if (!projectionFile)
	{
		std::cerr << "In msNormalisationTools.cpp: Could not open projection file. File not found. " << std::endl;
		return false;
	}
	// Warning: Don't try to close projectionFile. That's done already in the lambda 'openProjectionFile' above. 
	// Otherwise: "free(): double free detected in tcache 2 ... received signal SIGABRT, Aborted" may result.

	if (!openProjectionFile(projectionFile))
	{
		std::cerr << "In msNormalisationTools.cpp: Could not open projection file. " << std::endl;
		return false;
	}

	// 200515rh: This is a duplicate from: bool CFaceMarkDoc::OpenAugmentedDatasetFile(CString filename) (where mode_values** remains unused) - but mode_values are required for heatmap computation ...
	//            ... first half of above fct, however, loads into 'fields' from proj-file
	// ms: extract the mode values for each example
	{
		// find the mode1 column
		int iMode1Column = -1;
		for (int i = 0; i < N_CLASSIFICATIONS + 1; i++)
		{
			if (fields[0][i] == "mode1")
				iMode1Column = i;
		}
		if (iMode1Column == -1)
		{
			mfcGUImessage("Could not find 'mode1' column!");
			return false; //rh: Is this really critical and should be treated as if file could not be loaded correctly?
		}
		int N_MODES = N_CLASSIFICATIONS + 1 - iMode1Column;
		if (this->mode_values != NULL)
			mfcGUImessage("mode_value already allocated - memory leakage will result. (not fatal)");
		this->mode_values = new vtkDoubleArray * [this->N_EXAMPLES];
		float val;
		for (int i = 0; i < N_EXAMPLES; i++)
		{
			this->mode_values[i] = vtkDoubleArray::New();
			this->mode_values[i]->SetNumberOfTuples(N_MODES);

			for (int j = 0; j < N_MODES; j++)
			{
				val = std::stof(fields[i + 1][j + iMode1Column]);//sscanf(fields[i+1][j+iMode1Column],"%f",&val);
				this->mode_values[i]->SetValue(j, val);
			}
		}
	}
	return true;
}
