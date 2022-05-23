#include "classificationTools.h"
#include "../heatmapProcessing/vtkSurfacePCA.h"

//#include <vtkAutoInit.h>
//VTK_MODULE_INIT(vtkRenderingOpenGL);
//VTK_MODULE_INIT(vtkInteractionStyle); 
//VTK_MODULE_INIT(vtkRenderingFreeType);
#include "vtkSmartPointer.h"
#include "vtkPolyDataReader.h"
 #include <sys/stat.h>

#include "CFloatMatrix.h"
//Graph stuff
#include "vtkContextView.h"
#include "vtkChartXY.h"
#include "vtkTable.h"
#include "vtkFloatArray.h"
#include "vtkPlot.h"
#include "vtkPlotPoints.h"
#include "vtkRenderWindow.h"
#include "vtkRendererCollection.h"
#include "vtkRenderer.h"
#include "vtkAxis.h"
#include "vtkContextScene.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkPNGReader.h"
#include "vtkWindowToImageFilter.h"
#include "vtkTriangleFilter.h"
#include "vtkImageActor.h"
#include <iterator>


#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

using namespace std;

// Free function for std::string formatting a la printf, replacing call to .Format method on MFC CStrings
// May become obsolete when 'fields' datastructure is replaced.
// Duplicate from msNormalisationTools.cpp
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


ClassificationTools::ClassificationTools()
{
	this->N_SPLITS = 20; //should probably be defined by the number of split folders found //TODO!
	this->projected_table.clear();
	this->CMValues = std::vector<float>(this->N_SPLITS, 0.0F);
}

ClassificationTools::~ClassificationTools()
{
}

//rh: Function appears to be unused: Will not add error handling.
void ClassificationTools::ProjectIndividualInSplit(CString model_filename, vtkSmartPointer<vtkPolyData> surface, vtkSmartPointer<vtkPolyData> landmarks, int split_num)
{   
	vtkSurfacePCA *pca = vtkSurfacePCA::New();
	{
		bool ret = pca->LoadFile(model_filename);
        
		if(!ret) 
		{   
			// model failed to load! (possibly wrong type)
			//AfxMessageBox("Model failed to load. Aborting.");
			return;
		}
	} 
	const int n_modes = pca->GetTotalNumModes();
	//initialise  
 
	if(surface->GetNumberOfPoints()<=0) { std::cerr << "In ClassificationTools::ProjectIndividualInSplit: Failed to read surface correctly! Results may be wrong" << std::endl; }
	if(landmarks->GetNumberOfPoints()<=0) { std::cerr << "In ClassificationTools::ProjectIndividualInSplit: Failed to read landmarks correctly! Results may be wrong" << std::endl; }
	
	vtkSmartPointer<vtkDoubleArray> b = vtkSmartPointer<vtkDoubleArray>::New();
    //get mode values
	pca->GetApproximateShapeParameters(surface, landmarks, b, true);
	//add values to modes table
	pca->Delete();
  
	//add values to modes table 
	this->projected_table[split_num].reserve(static_cast<size_t>(n_modes));
	for(int j=0;j<n_modes;j++) 
		this->projected_table[split_num].push_back(b->GetValue(j)); 
	
	CString root_folder = model_filename;
#ifdef FILESDATASTRUCTUREISSORTEDOUT
    root_folder = root_folder.Left(root_folder.ReverseFind('\\'));
    root_folder = root_folder.Left(root_folder.ReverseFind('\\')+1);
#endif	
	this->OnClassifyIndividualsUsingClosestMean(root_folder, split_num);
}

bool ClassificationTools::ProjectResampledIndividualInSplit(const filesystem::path root_folder, const filesystem::path model_filename, vtkSmartPointer<vtkPolyData> surface, int split_num)
{   
	vtkNew<vtkSurfacePCA> pca;
	{
		bool ret = pca->LoadFile(model_filename.string());        
		if(!ret) { // possibly wrong type
			std::cerr << "In ClassificationTools::ProjectResampledIndividualInSplit: Model " << model_filename <<
				" failed to load. Aborting." << std::endl;
			return false;
		}
	} 
	 
	if(surface->GetNumberOfPoints()<=0) 
	{ 
		std::cerr << "In ClassificationTools::ProjectResampledIndividualInSplit: Failed to read surface correctly! Results may be wrong" << std::endl; 
		// TODO pass back error message to http_response
		return false;
	}
	
	//get mode values
	vtkSmartPointer<vtkDoubleArray> b = vtkSmartPointer<vtkDoubleArray>::New();
	//pca->GetApproximateShapeParameters(surface, landmarks, b, true);
	pca->GetApproximateShapeParametersFromResampledSurface(surface, b, true); 

	//add values to modes table
	const int n_modes = pca->GetTotalNumModes();
	this->projected_table[split_num].reserve(static_cast<size_t>(n_modes));
	for (int j = 0; j < n_modes; j++)
	{
		this->projected_table[split_num].push_back(b->GetValue(j));
	}

	const bool classificationSuccessful = this->OnClassifyIndividualsUsingClosestMean(root_folder, split_num); 
	return classificationSuccessful;
}
 
bool ClassificationTools::OnProjectIndividualsInSplitFolders(const std::filesystem::path root_folder, const vtkSmartPointer<vtkPolyData> example_surface, vtkSmartPointer<vtkPolyData> example_landmarks, float &meanRetVal, float &standardErrRetVal)
{    
	//check if all desired models and dat files can be found
//	#pragma omp parallel for
	for(int i = 0; i < this->N_SPLITS; i++)
	{
		//std::string  model_name = string_format("%s\\split%02d\\model.csv",root_folder.c_str(), i + 1); //string_format(const std::string& format, Args ... args)
		//std::string trainingdat = string_format("%s\\split%02d\\training.dat",root_folder.c_str(), i + 1);
		const std::filesystem::path splitDirName(string_format("split%02d", i + 1));
		const std::filesystem::path model_filename = root_folder / splitDirName / "model.csv";
		const std::filesystem::path trainingdata_filename = root_folder / splitDirName / "training.dat";

		struct stat buffer;   
		if (stat(model_filename.string().c_str(), &buffer) != 0)
		{
			std::cerr << "In ClassificationTools::OnProjectIndividualsInSplitFolders: Cannot find model file: " << model_filename << std::endl;
		}
		if (stat(trainingdata_filename.string().c_str(), &buffer) != 0)
		{
			std::cerr << "In ClassificationTools::OnProjectIndividualsInSplitFolders: Cannot find training.dat file: " << trainingdata_filename << std::endl;
		}
	}
	
	/* 
	for(const auto& split : projected_table)
	{
		split.clear();	
	}
	*/
	projected_table.clear();

	// New projected table [split_num][mode]
	this->projected_table.reserve(this->N_SPLITS);
	for (size_t i = 0; i < this->N_SPLITS; ++i)
	{
		this->projected_table.push_back(std::vector<float>{});
	}
	 
	// determine batch size
	//set params for each split
	
	//#pragma omp parallel for	
	//resample the surface using the first split model so we don't have to do this over and over again
 
 	vtkSmartPointer<vtkPolyData> resampled_surface = vtkSmartPointer<vtkPolyData>::New();
	const std::filesystem::path splitDirName(string_format("split%02d", 1));
	const std::filesystem::path model_filename = root_folder / splitDirName / "model.csv";
	vtkSurfacePCA *pca = vtkSurfacePCA::New();
	{
		bool ret = pca->LoadFile(model_filename.string());
        
		if(!ret) { // model failed to load! (possibly wrong type)
			std::cerr << "In ClassificationTools::OnProjectIndividualsInSplitFolders:Model failed to load. Aborting: " << model_filename<< std::endl;
			return false;
		}
		//check number of landmarks
		if(example_landmarks->GetNumberOfPoints() != pca->Getnlandmarks())
		{
			std::cerr << "Model failed: Wrong number of landmarks, example has " << example_landmarks->GetNumberOfPoints() << ", model has " << pca->Getnlandmarks() << std::endl;
			return false;
		}
		 
		vtkSmartPointer<vtkTriangleFilter> tri = vtkSmartPointer<vtkTriangleFilter>::New();
		tri->SetInputData(example_surface);
		tri->Update();

		// resample the supplied surface using the base mesh   
		pca->Resample(tri->GetOutput(), example_landmarks, resampled_surface); // rh: note: example_landmarks should be vtkPointSet*, not vtkPolyData*
	} 
	pca->Delete();
	 
	bool classificationSuccessful = true;
	#pragma omp parallel for
	for(int split=0;split<this->N_SPLITS;split++)
	{ 	 
		const std::filesystem::path splitDirName(string_format("split%02d", split + 1));
		const std::filesystem::path model_filename = root_folder / splitDirName / "model.csv";

	 	vtkSmartPointer<vtkPolyData> landmarks = vtkSmartPointer<vtkPolyData>::New();
	 	landmarks->DeepCopy(example_landmarks);
		
		vtkSmartPointer<vtkPolyData> surface = vtkSmartPointer<vtkPolyData>::New();
		surface->DeepCopy(resampled_surface);
		
		if (!(this->ProjectResampledIndividualInSplit(root_folder, model_filename, surface, split)))
		{
			classificationSuccessful = false; // Cannot return directly from OMP structured block.
		}
		
		//this->ProjectIndividualInSplit(model_filename, surface, landmarks, split);
	}
 
	//Needs to be set back as vtkResampler sets to 1
    	vtkDataObject::SetGlobalReleaseDataFlag(0);
	float mean = 0.0;
	//calulate mean and SD
	for(int split = 0; split < this->N_SPLITS; split++)  
		mean += this->CMValues[split];
 
	mean /= this->N_SPLITS;
	//calulate SD
	float SD = 0.0F;
	// #pragma omp parallel for
		for(int split = 0; split < this->N_SPLITS; split++)
			SD += pow(this->CMValues[split] - mean, 2);

	SD = sqrt( SD / (this->N_SPLITS-1));

	standardErrRetVal = SD/sqrt((float)this->N_SPLITS) * 1.96; // TODO: explain the double literal
	
	meanRetVal = mean;

	return classificationSuccessful; // false, iff one of the splits did not succeed to classify.
	
	//this->DrawGraph(mean, standardErr,"","",individual, thumbnailImage);
}

bool ClassificationTools::OnClassifyIndividualsUsingClosestMean(const filesystem::path root_folder, int split)
{
	// compute the m1m2 line for each split, 
	// classify test cases and individuals and output their classification
	// position to one big file   

	// firstly output the individuals, as their dataset.csv plus a column for the classification axis   
	// read-in the training.dat file (n examples, each with m modes)
	// and compute the m1m2 line
	// then read-in the test.dat file and compute for each example which mean is closer
	// output to summary.csv the actual and predicted classes and the sens and spec summary
	 
	std::unique_ptr <CFloatMatrix> pos_av; // CFloatMatrix pos_av,neg_av -- each is a n_modes*1 vector
	std::unique_ptr <CFloatMatrix> neg_av;
	std::unique_ptr <CFloatMatrix> m1m2; // the meanline vector
	std::unique_ptr <CFloatMatrix> D;
	std::unique_ptr <CFloatMatrix> m;

	// Reading mode values and labels file
	const std::filesystem::path splitDirName(string_format("split%02d", split + 1));
	const std::filesystem::path training_file_name = root_folder / splitDirName / "training.dat";
	std::ifstream inFile(training_file_name); 
	std::string line;
	bool readingFirstLine(true);
	size_t n_pos(0), n_neg(0);
	int numberOfModes(0);
	while (std::getline(inFile, line))
	{
		std::istringstream iss(line);   
		std::string class_label; // should be in {"-1", "1"}
		if (!(iss >> class_label)) 
		{ 
			break; // TODO: error reading file message
			return false;
		} 

		std::string mode_value_pair;
		std::vector<float> modeValues;
		while (iss >> mode_value_pair)
		{
			std::stringstream mode_value_pair_stream(mode_value_pair);
			std::string mode_item;
			std::getline(mode_value_pair_stream, mode_item, ':');
			size_t mode = std::stoi(mode_item);		// ASSERT mode == number_of_modes
			std::string value_item;
			std::getline(mode_value_pair_stream, value_item, ':');
			float value = std::stof(value_item);
			modeValues.push_back(value);
		}
										
		if (readingFirstLine)
		{
			numberOfModes = static_cast<int>(modeValues.size());
			pos_av = std::make_unique<CFloatMatrix>(numberOfModes, 1);
			neg_av = std::make_unique<CFloatMatrix>(numberOfModes, 1);
			pos_av->MakeZero();
			neg_av->MakeZero();
			D = std::make_unique<CFloatMatrix>(numberOfModes, 1); // D = CFloatMatrix(numberOfModes, 1);
			readingFirstLine = false;
		}
		for (int i = 0; i < numberOfModes; i++)
		{
			D->set(i, 0, modeValues.at(i));
		}

		if (class_label.compare("1") == 0)
		{ 
			n_pos++;
			*pos_av += *D; 
		}
		else if (class_label.compare("-1") == 0) 
		{ 
	    		n_neg++;
			*neg_av += *D; 
		}
		else
		{
			// TODO Error message: invalid class labels in projection file
			return false;
		}
	}

	inFile.close();

	*neg_av /= n_neg;
	*pos_av /= n_pos;

	// compute the meanline vector for comparison
	m1m2 = std::make_unique<CFloatMatrix>(numberOfModes, 1);
	*m1m2 = *pos_av - *neg_av;	

	// read-in the test set and classify as we go			 
	m = std::make_unique<CFloatMatrix>(numberOfModes, 1);	// CFloatMatrix m(numberOfModes, 1);
	// read each mode value in turn
	for (int mode = 0; mode < numberOfModes; mode++)
	{
		m->set(mode, 0, this->projected_table[split][mode]);
	}

	*m -= *neg_av;
	const auto classificationMeanValue = (2.0F * DotProduct(*m1m2, *m) / DotProduct(*m1m2, *m1m2)) - 1.0F;
	this->CMValues[split] = classificationMeanValue;
	return true;
}

// 200702 rh: Currently unused. Functionality reproduced with Latex tikz for report generation.
void ClassificationTools::DrawGraph(float value, float standardErr, CString individual, CString thumbnailImage, bool offScreenRender, vtkSmartPointer<vtkImageData> &imageData)
{
  // Set up a 2D scene, add an XY chart to it
  vtkSmartPointer<vtkContextView> view =
    vtkSmartPointer<vtkContextView>::New();
  view->GetRenderer()->SetBackground(1.0, 1.0, 1.0);
  view->GetRenderWindow()->SetSize(600, 150);

  vtkSmartPointer<vtkChartXY> chart =
    vtkSmartPointer<vtkChartXY>::New();

  view->GetScene()->AddItem(chart);

  chart->SetShowLegend(false);

  vtkStdString title = "Classification for "+ individual;
  chart->SetTitle(title);
  chart->GetAxis(0)->SetBehavior(vtkAxis::FIXED);
  chart->GetAxis(0)->SetRange(0.0,0.5);
  chart->GetAxis(0)->SetPoint1(0,0);
  //chart->GetAxis(0)->SetOpacity(1);
  chart->GetAxis(0)->SetLabelsVisible(false);
  chart->GetAxis(0)->SetTitle("");

  std::string controlFas = string_format("%s                                              %s", "CONTROL", "FAS");
  vtkStdString FASCONTOL = controlFas;
  
  chart->GetAxis(1)->SetTitle(FASCONTOL);
  chart->GetAxis(1)->SetBehavior(vtkAxis::FIXED); 
  chart->GetAxis(1)->SetRange(-3, 3);
   // Create a table with some points in it...
  vtkSmartPointer<vtkTable> table =
    vtkSmartPointer<vtkTable>::New();
 
  vtkSmartPointer<vtkFloatArray> arrX =
    vtkSmartPointer<vtkFloatArray>::New();
  arrX->SetName("CONTROL_FAS");
  table->AddColumn(arrX);
 
  vtkSmartPointer<vtkFloatArray> point =
    vtkSmartPointer<vtkFloatArray>::New();
  point->SetName(individual.c_str());
  table->AddColumn(point);
   
  vtkSmartPointer<vtkFloatArray> upper_err =
    vtkSmartPointer<vtkFloatArray>::New();
  upper_err->SetName("Upper limit");
  table->AddColumn(upper_err);
   
  vtkSmartPointer<vtkFloatArray> lower_err =
    vtkSmartPointer<vtkFloatArray>::New();
  lower_err->SetName("Lower limit");
  table->AddColumn(lower_err);
 
  // Test charting with a few more points...
  int numPoints = 3;
  table->SetNumberOfRows(numPoints);

  table->SetValue(0, 0, value);
  table->SetValue(0, 1, 0.25);

  table->SetValue(1, 0, value+standardErr);
  table->SetValue(1, 2, 0.25);

  table->SetValue(2, 0, value-standardErr);
  table->SetValue(2, 3, 0.25);

  // Add multiple scatter plots, setting the colors etc
  vtkPlot *points = chart->AddPlot(vtkChart::POINTS);
  points->SetInputData(table, 0, 1);
  points->SetColor(0, 0, 0, 255);
  points->SetWidth(1.0);
  vtkPlotPoints::SafeDownCast(points)->SetMarkerStyle(vtkPlotPoints::DIAMOND);
 
  points = chart->AddPlot(vtkChart::POINTS);

  points->SetInputData(table, 0, 2);
  points->SetColor(0, 0, 0, 255);
  points->SetWidth(1.0);
  vtkPlotPoints::SafeDownCast(points)->SetMarkerStyle(vtkPlotPoints::CROSS);

  points = chart->AddPlot(vtkChart::POINTS);

  points->SetInputData(table, 0, 3);
  points->SetColor(0, 0, 255, 255);
  points->SetWidth(1.0);
  vtkPlotPoints::SafeDownCast(points)->SetMarkerStyle(vtkPlotPoints::CROSS);
   
  view->GetRenderWindow()->SetMultiSamples(0);
  view->GetRenderWindow()->Render();
  if(!offScreenRender)
  {
	view->GetInteractor()->Initialize();
	view->GetInteractor()->Start();
  }
  else if(imageData)
  {
	  // Screenshot  
	  vtkNew<vtkWindowToImageFilter> windowToImageFilter;
	  windowToImageFilter->SetInput(view->GetRenderWindow());
	  windowToImageFilter->SetScale(1, 1); //set the resolution of the output image (3 times the current resolution of vtk render window)
	 // windowToImageFilter->SetInputBufferTypeToRGBA(); //also record the alpha (transparency) channel
	  windowToImageFilter->SetInputBufferTypeToRGB();
	  windowToImageFilter->ReadFrontBufferOff(); // read from the back buffer
	  windowToImageFilter->Update();
	  imageData->DeepCopy(windowToImageFilter->GetOutput());   
  }
}
