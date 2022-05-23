#include "faceScreenServerDefinitions.h"

#include "faceScreeningObject.h"
#include "heatmapProcessing/msNormalisationTools.h"
#include "heatmapProcessing/vtkSurfacePCA.h"
#include "subjectClassification/classificationTools.h"
#include "PFLcomputation/msPFLMeasure.h"

#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <map>
#include <vector>
#include <iostream>     // std::cout
#include <fstream>      // std::ifstream

#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkSmartPointer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkScalarBarActor.h>
#include <vtkSphereSource.h>
#include <vtkWindowToImageFilter.h>
#include <vtkPNGWriter.h>
#include <vtkLandmarkTransform.h> // for heatmap registration
#include <vtkTransformPolyDataFilter.h> // for heatmap registration
#include <vtkLookupTable.h>
#include <vtkRenderLargeImage.h>
#include <vtkXMLImageDataWriter.h>
#include <vtkImageCast.h>
#include <vtkJPEGWriter.h>

#include <cpprest/asyncrt_utils.h>
#include <cpprest/rawptrstream.h>

FaceScreeningObject::FaceScreeningObject()
{
	creationTimestamp = utility::datetime::utc_timestamp(); // Unix/POSIX time since 01-01-1970
	busyComputing = false; // Better? : Check http_response has been sent with certain time after request, or timeout?
}

void FaceScreeningObject::debugViz(vtkSmartPointer<vtkPolyData> mesh, vtkSmartPointer<vtkTexture> facialTexture)
{
	vtkNew<vtkPolyDataMapper> mapper;
	mapper->SetInputData(mesh);
	double scalarRange[2] = { -2, 2 };
	vtkNew<vtkLookupTable> lut;
	lut->SetHueRange(0.66, 0); // red to blue
	lut->SetValueRange(1, 1);
	lut->SetSaturationRange(1, 1);
	lut->SetAlphaRange(1, 1);
	lut->SetRampToLinear();
	lut->SetScaleToLinear();
	lut->SetTableRange(scalarRange); // 
	lut->Build();
	mapper->SetLookupTable(lut);
	mapper->SetScalarRange(scalarRange);
	vtkNew<vtkActor> actor;
	actor->SetMapper(mapper);
	actor->SetTexture(facialTexture);
	vtkNew<vtkRenderer> renderer;
	vtkNew<vtkRenderWindow> renderWindow;
	renderWindow->AddRenderer(renderer);
	renderWindow->SetAlphaBitPlanes(1); //enable usage of alpha channel
	vtkNew<vtkRenderWindowInteractor> renderWindowInteractor;
	renderWindowInteractor->SetRenderWindow(renderWindow);
	renderer->AddActor(actor);
	renderer->SetBackground(1, 1, 1); // Background color white
	renderWindow->Render();  
	renderWindowInteractor->Start();
}

void FaceScreeningObject::renderToJpgFile(vtkSmartPointer<vtkPolyData> graphicObject, vtkSmartPointer<vtkTexture> facialTexture, std::string tmpImageFilename, std::map<std::string, Point3D> landmarks, bool renderProfile, bool renderScale)
{
	if (!graphicObject->GetNumberOfCells() > 0)
	{
		return;
	}

	vtkNew<vtkPolyDataMapper> mapper;
	mapper->SetInputData(graphicObject);

	vtkNew<vtkActor> actor;
	actor->SetMapper(mapper);
	if (facialTexture != nullptr)
	{
		actor->SetTexture(facialTexture);
	}

	vtkNew<vtkRenderer> renderer;
	
	if (renderProfile && !landmarks.empty() && landmarks.count("subnasale") > 0 && landmarks.count("left_ex") > 0 && landmarks.count("right_ex") > 0)
	{
		const auto subnasale = landmarks.at("subnasale");
		const auto PROJ_VALUE = -250.F;
		try
		{
			const auto exR = landmarks.at("right_ex");
			const auto exL = landmarks.at("left_ex");
			const auto axisX = exR.x - exL.x;
			const auto axisY = exR.y - exL.y;
			const auto axisZ = exR.z - exL.z;
			const auto normalizationFactor = std::sqrt((axisX * axisX) + (axisY * axisY) + (axisZ * axisZ));
			const auto normedAxisX = axisX / normalizationFactor;
			const auto normedAxisY = axisY / normalizationFactor;
			const auto normedAxisZ = axisZ / normalizationFactor;
			const double cameraCoords[3] = { subnasale.x + (normedAxisX * PROJ_VALUE),  subnasale.y + (normedAxisY * PROJ_VALUE), subnasale.z + (normedAxisZ * PROJ_VALUE) };
			const auto camera = renderer->GetActiveCamera();
			camera->SetPosition(cameraCoords);
		}
		catch(const std::exception& ex)
		{
			std::cerr << "Error in rendering profile image: Camera coordinates could not be set due to missing landmarks right_ex or left_ex (required for registration). " 
			<< ex.what() << std::endl;
			return;
		}
	}

	if (renderScale) // for, e.g., heatmaps
	{
		double scalarRange[2] = { -2, 2 };
		vtkNew<vtkLookupTable> lut;
		lut->SetHueRange(0.66, 0); // red to blue
		lut->SetValueRange(1, 1);
		lut->SetSaturationRange(1, 1);
		lut->SetAlphaRange(1, 1);
		lut->SetRampToLinear();
		lut->SetScaleToLinear();
		lut->SetTableRange(scalarRange); 
		lut->Build();
		mapper->SetLookupTable(lut);
		mapper->SetScalarRange(scalarRange);

		vtkNew<vtkScalarBarActor> sbar;
		sbar->SetLookupTable(mapper->GetLookupTable());
		sbar->SetTitle("Stdv");
		sbar->SetNumberOfLabels(5);
		renderer->AddActor(sbar);
	}

	double greenValue = 1.0;
	for (const auto& lm : landmarks)
	{
		vtkNew<vtkSphereSource> landmark;
		landmark->SetRadius(1.2);
		landmark->SetCenter(lm.second.x, lm.second.y, lm.second.z);

		vtkNew<vtkPolyDataMapper> mapper;
		mapper->AddInputConnection(landmark->GetOutputPort());

		vtkNew<vtkActor> actor;
		actor->SetMapper(mapper);
		actor->GetProperty()->SetColor(0.0, greenValue, 0.0); //(R,G,B)
		greenValue -= 0.01;

		renderer->AddActor(actor);
	}

	vtkNew<vtkRenderWindow> renderWindow;
	renderWindow->AddRenderer(renderer);
	renderWindow->SetAlphaBitPlanes(1); //enable usage of alpha channel

	vtkNew<vtkRenderWindowInteractor> renderWindowInteractor;
	renderWindowInteractor->SetRenderWindow(renderWindow);
	renderer->AddActor(actor);
	renderer->SetBackground(1, 1, 1); // Background color white
	renderWindow->Render();  // the cast filter below does not work without this call

	vtkNew<vtkRenderLargeImage> lir;
	lir->SetInput(renderer);
	lir->Update();
	//vtkImageData* largeImage = lir->GetOutput();

	vtkNew<vtkImageCast> castFilter;
	castFilter->SetOutputScalarTypeToUnsignedChar();
	castFilter->SetInputConnection(lir->GetOutputPort());
	castFilter->Update();

	vtkNew<vtkJPEGWriter> writer;
	writer->SetFileName(tmpImageFilename.c_str());
	writer->SetInputConnection(castFilter->GetOutputPort());
	writer->Write();
}

vtkSmartPointer<vtkUnsignedCharArray> FaceScreeningObject::renderToJpg(vtkSmartPointer<vtkPolyData> graphicObject, vtkSmartPointer<vtkTexture> facialTexture, std::map<std::string, Point3D> landmarks, bool renderProfile)
{

	if (!graphicObject->GetNumberOfCells() > 0)
	{
		return vtkSmartPointer<vtkUnsignedCharArray>{};
	}
	
	vtkNew<vtkPolyDataMapper> mapper;
	mapper->SetInputData(graphicObject);

	vtkNew<vtkActor> actor;
	actor->SetMapper(mapper);
	if (facialTexture != nullptr)
	{
//		actor->SetTexture(facialTexture); TODO: check if binding texture works properly, to avoid OpenGL errors.
	}

	vtkNew<vtkRenderer> renderer;
	
	if (renderProfile && !landmarks.empty() && landmarks.count("subnasale") > 0 && landmarks.count("left_ex") > 0 && landmarks.count("right_ex") > 0)
	{
		const auto subnasale = landmarks.at("subnasale");
		const auto PROJ_VALUE = -600.F;
		try
		{
			const auto exR = landmarks.at("right_ex");
			const auto exL = landmarks.at("left_ex");
			const auto axisX = exR.x - exL.x;
			const auto axisY = exR.y - exL.y;
			const auto axisZ = exR.z - exL.z;
			const auto normalizationFactor = std::sqrt((axisX * axisX) + (axisY * axisY) + (axisZ * axisZ));
			const auto normedAxisX = axisX / normalizationFactor;
			const auto normedAxisY = axisY / normalizationFactor;
			const auto normedAxisZ = axisZ / normalizationFactor;
			const double cameraCoords[3] = { subnasale.x + (normedAxisX * PROJ_VALUE),  subnasale.y + (normedAxisY * PROJ_VALUE), subnasale.z + (normedAxisZ * PROJ_VALUE) };
			const auto camera = renderer->GetActiveCamera();
			camera->SetPosition(cameraCoords);
		}
		catch(const std::exception& ex)
		{
			std::cerr << "Error in rendering profile image: Camera coordinates could not be set due to missing landmarks right_ex or left_ex (required for registration). " 
			<< ex.what() << std::endl;
		}
	}

	// TODO For best visibility, facial landmarks might be light green if a texture is applied
	// ... blue otherwise.
	for (const auto& lm : landmarks)
	{
		vtkNew<vtkSphereSource> landmark;
		landmark->SetRadius(1.2);
		landmark->SetCenter(lm.second.x, lm.second.y, lm.second.z);

		vtkNew<vtkPolyDataMapper> mapper;
		mapper->AddInputConnection(landmark->GetOutputPort());

		vtkNew<vtkActor> actor;
		actor->SetMapper(mapper);
		actor->GetProperty()->SetColor(0.0, 0.0, 1.0); //(R,G,B) 

		renderer->AddActor(actor);
	}

	vtkNew<vtkRenderWindow> renderWindow;
	renderWindow->AddRenderer(renderer);
	renderWindow->SetAlphaBitPlanes(1); //enable usage of alpha channel

	vtkNew<vtkRenderWindowInteractor> renderWindowInteractor;
	renderWindowInteractor->SetRenderWindow(renderWindow);
	renderer->AddActor(actor);
	renderer->SetBackground(1, 1, 1); // Background color white
	renderWindow->Render();  // the cast filter below does not work without this call

	vtkNew<vtkRenderLargeImage> lir;
	lir->SetInput(renderer);
	lir->Update();
	//vtkImageData* largeImage = lir->GetOutput();

	vtkNew<vtkImageCast> castFilter;
	castFilter->SetOutputScalarTypeToUnsignedChar();
	castFilter->SetInputConnection(lir->GetOutputPort());
	castFilter->Update();

	vtkNew<vtkJPEGWriter> writer;
	writer->WriteToMemoryOn();
	writer->SetInputConnection(castFilter->GetOutputPort());
	writer->Write();
	return writer->GetResult();
}

vtkSmartPointer<vtkUnsignedCharArray> FaceScreeningObject::renderToJpg(vtkSmartPointer<vtkPolyData> surface)
{
	if (!surface->GetNumberOfCells() > 0)
	{
		return vtkSmartPointer<vtkUnsignedCharArray>{};
	}

	vtkNew<vtkPolyDataMapper> mapper;
	mapper->SetInputData(surface);
	double scalarRange[2] = { -2, 2 };
	vtkNew<vtkLookupTable> lut;
	lut->SetHueRange(0.66, 0); // red to blue
	lut->SetValueRange(1, 1);
	lut->SetSaturationRange(1, 1);
	lut->SetAlphaRange(1, 1);
	lut->SetRampToLinear();
	lut->SetScaleToLinear();
	lut->SetTableRange(scalarRange); // 
	lut->Build();
	mapper->SetLookupTable(lut);
	mapper->SetScalarRange(scalarRange);

	vtkNew<vtkActor> actor;
	actor->SetMapper(mapper);

	vtkNew<vtkRenderer> renderer;
	vtkNew<vtkRenderWindow> renderWindow;
	renderWindow->AddRenderer(renderer);
	renderWindow->SetAlphaBitPlanes(1); //enable usage of alpha channel

	vtkNew<vtkRenderWindowInteractor> renderWindowInteractor;
	renderWindowInteractor->SetRenderWindow(renderWindow);
	renderer->AddActor(actor);
	renderer->SetBackground(1, 1, 1); // Background color white
	renderWindow->Render();  // the cast filter below does not work without this call

	vtkNew<vtkRenderLargeImage> lir;
	lir->SetInput(renderer);
	lir->Update();
	vtkImageData* largeImage = lir->GetOutput();
	vtkNew<vtkDoubleArray> imageData;

	vtkNew<vtkImageCast> castFilter;
	castFilter->SetOutputScalarTypeToUnsignedChar();
	castFilter->SetInputConnection(lir->GetOutputPort());
	castFilter->Update();

	vtkNew<vtkJPEGWriter> writer;
	writer->WriteToMemoryOn();
	//writer->SetResult(this->pngHeatmap);				// ISSUES  H E R E   ! ! ! (solved below)
	writer->SetInputConnection(castFilter->GetOutputPort());
	writer->Write();

	return writer->GetResult();
}

// Decodes json string into std::map of facial landmarks, stores a copy of this in VTK compatible format
// Returns the type of landmark set identified from the modelDB server config file.
utility::string_t FaceScreeningObject::parseLandmarks(const web::json::value& landmarksASjson, const web::json::value& landmarksSetTypesFromModelDB)
{
	utility::string_t identifiedLandmarkSetType; // return value; remains empty if landmark set could not be identified

	std::vector<std::string> userProvidedLandmarkNames;

	for (auto const& landmark_iter : landmarksASjson.as_object())
	{
		const utility::string_t& landmarkName = landmark_iter.first;
		const web::json::value& landmarkCoordinates = landmark_iter.second;

		if (landmarkCoordinates.size() != 3)
		{
			ucout << "Landmark " << landmarkName
				<< " does not have exactly 3 coordintes. Ignoring landmark. " << endl;
			continue;
		}

		try 
		{
			const auto x = landmarkCoordinates.at(0).as_number().to_double();
			const auto y = landmarkCoordinates.at(1).as_number().to_double();
			const auto z = landmarkCoordinates.at(2).as_number().to_double();
			std::string lmKey = utility::conversions::to_utf8string(landmarkName);
			landmarks[lmKey] = { x, y, z }; //Point3D
			userProvidedLandmarkNames.push_back(lmKey);
		}
		catch (const web::json::json_exception& ex)
		{
			ucout << "Landmark coordinate conversion of landmark " << landmarkName
				<< " failed due to unsuitable data type in one of the point coordinates: " << ex.what() << endl;
			return identifiedLandmarkSetType; // empty return for indicating no match to landmark sets
		}
	}

	// Find the type of landmark set (defined in file modelDB.json) matches the user/client input.
	// Current implementation finds the first instance that fits, not necessarily the largest one. Could be TODO.
	bool landmarkSetTypeFound(false);
	for (auto const& landmarkSetType_iter : landmarksSetTypesFromModelDB.as_object())
	{
		const auto& lmSetType(landmarkSetType_iter.second.as_array());
		std::vector<string> modelLandmarkNames;
		for (const auto& lm : lmSetType)
		{
			modelLandmarkNames.push_back(utility::conversions::to_utf8string(lm.as_string()));
		}
		std::sort(modelLandmarkNames.begin(),modelLandmarkNames.end());
		std::sort(userProvidedLandmarkNames.begin(), userProvidedLandmarkNames.end());

		// For some reason, std::includes does not seem to work correctly ...
		// landmarkSetTypeFound = std::includes(userProvidedLandmarkNames.begin(), userProvidedLandmarkNames.end(), modelLandmarkNames.begin(), modelLandmarkNames.end(),
		//	[](std::string a, std::string b) { return a.compare(b) == 0; });
		// ... so, implement subset check manually:
		landmarkSetTypeFound = std::all_of(modelLandmarkNames.cbegin(), modelLandmarkNames.cend(), [&userProvidedLandmarkNames](const auto& modelLMname) 
			{return std::find_if(userProvidedLandmarkNames.cbegin(), userProvidedLandmarkNames.cend(), [&modelLMname](const auto& usrLMname) 
				{return modelLMname.compare(usrLMname) == 0;}) != userProvidedLandmarkNames.cend();} );
		
		if (landmarkSetTypeFound)
		{
			identifiedLandmarkSetType = landmarkSetType_iter.first;
			break;
		}
	}

	if (!landmarkSetTypeFound)
	{
		return identifiedLandmarkSetType; // empty return for indicating no match to landmark sets
	}

	// Convert landmark coordinates to VTK format. 
	// Into this datastructure, insert only the subset of user submitted landmarks that were used to create the model. 
	// This is relevant if more landmarks were submitted than required.
	// Warning: When converting landmarks type map into names vector, we cannot use the one created above, because it is sorted, and the original order is required!
	const web::json::value identifiedLandmarkSet = landmarksSetTypesFromModelDB.at(identifiedLandmarkSetType);
	orderedLandmarkNames.clear();
	orderedLandmarkNames.reserve(identifiedLandmarkSet.size());
	for (const auto& landmarkName : identifiedLandmarkSet.as_array())
	{
		orderedLandmarkNames.push_back(utility::conversions::to_utf8string(landmarkName.as_string()));
	}

	// Erase all user submitted landmarks not required to work with the identified model. 
	// It will be necessary to copy these into a seperate datastruct once a model can be selected directly by API clients, instead of being selected automatically as implemented above.
	// Erase landmark from user submitted set, if it's name is not in the list of the landmarkSetType identified above
	// WARNING: This erasure needs to be done as below, so as not to invalidate the iterator.
	auto it = landmarks.cbegin();
	while (it != landmarks.cend())
	{
		if (std::find(orderedLandmarkNames.cbegin(), orderedLandmarkNames.cend(), it->first) == orderedLandmarkNames.cend())
		{
			it = landmarks.erase(it);
		}
		else
		{
			++it;
		}
	}

	// Convert landmarks to format expected by VTK.
	vtkNew<vtkPolyData> landmarks_InVTKFormat_TMP;
	vtkNew<vtkPoints> landmark_points_tmp;
	landmarks_InVTKFormat_TMP->Allocate(this->landmarks.size());  
	// Use order as defined in modelDB.json (LandmarkSetType).
	try
	{
		for(long long int index = 0; index < orderedLandmarkNames.size(); index++)
		{
			const auto nextLandmark = landmarks.at(orderedLandmarkNames.at(index)); 
			landmark_points_tmp->InsertNextPoint(nextLandmark.x, nextLandmark.y, nextLandmark.z);
			landmarks_InVTKFormat_TMP->InsertNextCell(VTK_VERTEX, 1, &index /*originally: vtkIdType*/);   
		}
		landmarks_InVTKFormat_TMP->SetPoints(landmark_points_tmp);
		landmarks_InVTKFormat = landmarks_InVTKFormat_TMP;
		std::cout << "Landmark parsing successful. Identified LANDMARK_SETTYPE: " << identifiedLandmarkSetType << std::endl;
	}
	catch(const std::exception& e) // out_of_range exception if map element did not exist.
	{
		std::cout << "Internal error transforming landmarks into VTK format. This should not happen! "	<< std::endl;
		return utility::string_t(); // Empty return is magic value for: decoding failed.
	}

	return identifiedLandmarkSetType;
}

void FaceScreeningObject::computeHeatmap(const web::http::http_request& message, const std::filesystem::path modelFilesRootDir, const std::string ethnicity_code, const float subject_age)
{
	std::cout << "In FaceScreeningObject::computeHeatmap(...): modeFilesRootDir = " << modelFilesRootDir << std::endl;
	this->subjectAge = subject_age;

	// derived from: void CFaceMarkDoc::CalcualateFacialSignature(int example, vtkSmartPointer<vtkPolyData> signature)
	// TODO: 
	// - surface cleaning/stripping ? --> It appears this is not required.
	// - speed up model file loading
	// - speed up resample function

	const filesystem::path model_FileName = modelFilesRootDir / filesystem::path("model.dat");

	vtkNew<vtkSurfacePCA> pca;
	if (!pca->LoadFile(model_FileName.string()))
	{
		cerr << "In FaceScreeningObject::computeHeatmap() : Failed to load model file." << endl;
		message_reply(web::http::status_codes::NotFound, U("Face model file could not be loaded."));
		return;
	}

	if (this->surfaceMesh->GetNumberOfPoints() <= 0) 
	{ 
		cerr << "In FaceScreeningObject::computeHeatmap(): Failed to read face mesh correctly! Results may be wrong" << endl; 
		message_reply(web::http::status_codes::NotFound, U("The face mesh of the subject is not available."));
		return; 
	}
	if (landmarks_InVTKFormat->GetNumberOfPoints() <= 0)
	{ 
		cerr << "In FaceScreeningObject::computeHeatmap(): Failed to read landmarks correctly! Results may be wrong" << endl; 
		message_reply(web::http::status_codes::NotFound, U("The landmarks of the subject are not available."));
		return;
	}
	// This should compare against data in 'landmarks_InVTKFormat', and the 'landmarks' map!
	if (pca->Getnlandmarks() != this->landmarks.size())
	{
		cerr << "In FaceScreeningObject::computeHeatmap(): Wrong number of landmarks! Model uses " << pca->Getnlandmarks() << "but number of landmarks submitted is: " << landmarks.size() << endl;
		message_reply(web::http::status_codes::NotFound, U("The face model with the specified number of landmarks was not found."));
		return; 
	}

	// Compute shape params. Synthesize surface from model.
	// (Legacy code did surface cleaning and stripping at this point. TODO Check if necessary.)
	vtkNew<vtkDoubleArray> b;
	pca->GetApproximateShapeParameters(this->surfaceMesh, landmarks_InVTKFormat, b, true);

	vtkNew<vtkPolyData> signature;
	pca->ParameteriseShape(b, signature);

	vtkNew<vtkPolyData> landmarks_onParameterisedDSM;
	pca->GetParameterisedLandmarks(signature, landmarks_onParameterisedDSM); // rh: function is setting a field "current_landmarks" in vtkSurfacePCA. 
																			
	if (signature->GetNumberOfPoints() <= 0) 
	{ 
		cerr << "In FaceScreeningObject::computeHeatmap(): Error in reading face model parameters and generating reference face mesh." << endl; 
		message_reply(web::http::status_codes::NotFound, U("Error in reading face model parameters and generating reference face mesh."));
		return;
	}

	// For NORMALISATION: Load projection file (i.e., collection of subjects with diagnostic outcome) 
	const filesystem::path projection_FileName = modelFilesRootDir / filesystem::path("projection.csv");

	msNormalisationTools norm;
	norm.SetPCAModel(pca);
	if (!norm.LoadProjectionFile(projection_FileName.string()))
	{
		cerr << "In FaceScreeningObject::computeHeatmap() : Failed to load project file." << endl;
		message_reply(web::http::status_codes::NotFound, U("Projection file could not be loaded."));
		return;
	}

	const auto errorCode_calcMatchMeanSignificance = norm.CalculateMatchedMeanSignificance(signature, b, subject_age);
	if (errorCode_calcMatchMeanSignificance == -1) 
	{
		cerr << "In FaceScreeningObject::computeHeatmap(): computation failed : norm->CalculateMatchedMeanSignificance(...). Calculation failed: 'from_var' missing or not set to 'control'" << endl;
		message_reply(web::http::status_codes::NotFound, U("ComputeHeatmap/CalculateMatchedMeanSignificance(...):  Calculation failed: 'from_var' missing or not set to 'control'"));
		return; 
	}
	if (errorCode_calcMatchMeanSignificance == -2) 
	{
		cerr << "In FaceScreeningObject::computeHeatmap(): computation failed : norm->CalculateMatchedMeanSignificance(...). Calculation failed: N_refs < 2 (Insuficient reference surfaces)" << endl;
		message_reply(web::http::status_codes::NotFound, U("ComputeHeatmap/CalculateMatchedMeanSignificance(...):  Calculation failed: N_refs < 2 (Insuficient reference surfaces)"));
		return; 
	}
	if (errorCode_calcMatchMeanSignificance == -3) 
	{
		cerr << "In FaceScreeningObject::computeHeatmap(): computation failed : norm->CalculateMatchedMeanSignificance(...). Age column 'age' missing in projection file." << endl;
		message_reply(web::http::status_codes::NotFound, U("ComputeHeatmap/CalculateMatchedMeanSignificance(...): Age column 'age' missing in projection file."));
		return; 
	}
	if (errorCode_calcMatchMeanSignificance == -4) 
	{
		cerr << "In FaceScreeningObject::computeHeatmap(): computation failed : norm->CalculateMatchedMeanSignificance(...). Syndrome/Dx column 'Dx' missing in projection file." << endl;
		message_reply(web::http::status_codes::NotFound, U("ComputeHeatmap/CalculateMatchedMeanSignificance(...): Syndrome/Dx column 'Dx' missing in projection file."));
		return; 
	}

	// Transfrom DSM representation to match original image (helps with orientation issues!)
	// rh: TODO: check if completeness of corresponding landmarks have to be checked here again.  TODO: if not complete: just dont do the tranform!
	vtkNew<vtkPoints> sourcePoints; // DSM
	vtkNew<vtkPoints> targetPoints; // Subject

	// Selection based on landmark model requested through REST API.
	const std::array<string, 4> transformReferenceLandmarks = { "nasion", "gnathion", "right_ex", "left_ex"};
	bool landmarkCorrespondencesIncomplete(false);
	for(const auto& landmarkName : transformReferenceLandmarks)
	{
		const vector<string>::const_iterator it = find(orderedLandmarkNames.cbegin(), orderedLandmarkNames.cend(), landmarkName);
		if (it == std::end(orderedLandmarkNames))
		{
			landmarkCorrespondencesIncomplete = true;
		}
		const auto index = distance(orderedLandmarkNames.cbegin(), it);
		sourcePoints->InsertNextPoint(landmarks_onParameterisedDSM->GetPoint(index));
		const auto& targetLandmark = landmarks[landmarkName];
		const double targetLandmark_[3] = { targetLandmark.x, targetLandmark.y, targetLandmark.z };
		targetPoints->InsertNextPoint(targetLandmark_);
	}

	if (landmarkCorrespondencesIncomplete)
	{
		message_reply(web::http::status_codes::OK, U("Heatmap computed successfully, but orientation not registered to input mesh due to missing landmarks."));
	}

	vtkNew<vtkLandmarkTransform> landmarkTransform;
	landmarkTransform->SetSourceLandmarks(sourcePoints);
	landmarkTransform->SetTargetLandmarks(targetPoints);
	landmarkTransform->SetModeToRigidBody();
	landmarkTransform->Update();

	vtkNew<vtkTransformPolyDataFilter> transformFilter;
	transformFilter->SetInputData(signature);
	transformFilter->SetTransform(landmarkTransform);
	transformFilter->Update();

	const auto transformedMesh = transformFilter->GetOutput();
	heatmap = vtkSmartPointer<vtkPolyData>::New();
	this->heatmap->DeepCopy(transformedMesh); // rh TODO: Is this really necessary? Use move semantics?

	message_reply(web::http::status_codes::OK, U("Heatmap computed successfully."));
}

void FaceScreeningObject::renderHeatmapImage(const web::http::http_request& message)
{
	if (this->heatmap == nullptr)
	{
		message_reply(web::http::status_codes::NotFound, U("Heatmap has not yet been computed."));
		return;
	}
	
	this->jpgHeatmapImage = renderToJpg(this->heatmap);
	char* imageData = (char*)jpgHeatmapImage->GetVoidPointer(0);
	size_t imageSize = (size_t)(jpgHeatmapImage->GetSize() * jpgHeatmapImage->GetDataTypeSize());
	std::cout << "Writing JPEG to mem... size: " << imageSize << endl;

	std::vector<uint8_t> imageData2;
	imageData2.reserve(imageSize);
	for (int i = 0; i < imageSize; ++i)
	{
		imageData2.push_back(imageData[i]);
	}

	concurrency::streams::bytestream byteStream = concurrency::streams::bytestream();
	concurrency::streams::istream imageStream = byteStream.open_istream(imageData2);

//	message.reply(web::http::status_codes::OK, imageStream, _XPLATSTR("application/octet-stream"));
    	web::http::http_response response(web::http::status_codes::OK);
        response.headers().add(U("Access-Control-Allow-Origin"), CORS_PERMISSIONS);
        response.set_body(imageStream);	// Default param set: &content+type = _XPLATSTR("application/octet-stream")
        message.reply(response);
}

void FaceScreeningObject::renderPortraitImage(const web::http::http_request& message, const bool renderProfile)
{
	if (surfaceMesh == nullptr || facialTexture == nullptr)
	{
		message_reply(web::http::status_codes::NotFound, U("Portrait cannot be rendered: Mesh or texture data missing."));
		return;
	}
	
	const auto jpgImage = renderToJpg(surfaceMesh, facialTexture, landmarks, renderProfile);
	
	char* imageData = (char*)jpgImage->GetVoidPointer(0);
	size_t imageSize = (size_t)(jpgImage->GetSize() * jpgImage->GetDataTypeSize());
	std::cout << "Writing JPEG to mem... size: " << imageSize << endl;

	std::vector<uint8_t> imageData2;
	imageData2.reserve(imageSize);
	for (int i = 0; i < imageSize; ++i)
	{
		imageData2.push_back(imageData[i]);
	}

	concurrency::streams::bytestream byteStream = concurrency::streams::bytestream();
	concurrency::streams::istream imageStream = byteStream.open_istream(imageData2);

    	web::http::http_response response(web::http::status_codes::OK);
        response.headers().add(U("Access-Control-Allow-Origin"), CORS_PERMISSIONS);
        response.set_body(imageStream);
        message.reply(response);
}
					       
bool FaceScreeningObject::computeClassification(const web::http::http_request& message, const filesystem::path facialRegionModelDataPath, const std::string facialRegionName)
{	
	if (surfaceMesh == nullptr)
	{
		cerr << "In FaceScreeningObject::computeClassification() : Classification requires face surface mesh." << endl;
		message_reply(web::http::status_codes::NotFound, U("Classification requires face surface mesh to be uploaded first."));
		return false;
	}

	if (landmarks_InVTKFormat == nullptr)
	{
		cerr << "In FaceScreeningObject::computeClassification() : Classification requires landmarks to be uploaded first." << endl;
		message_reply(web::http::status_codes::NotFound, U(" Classification requires landmarks to be uploaded first"));
		return false;
	}

	closestMeanClassifiers.emplace(facialRegionName, ClassificationTools());
	closestMeans.emplace(facialRegionName, 0.0F);
	closestStdDevs.emplace(facialRegionName, 0.0F);
	if (closestMeanClassifiers.at(facialRegionName).OnProjectIndividualsInSplitFolders(facialRegionModelDataPath, surfaceMesh, landmarks_InVTKFormat, 
		closestMeans[facialRegionName], closestStdDevs[facialRegionName]))
	{
            closestMeanClassifications.insert(std::pair<std::string, classificationResult>(facialRegionName, {closestMeans[facialRegionName], closestStdDevs[facialRegionName]}));
            message_reply(web::http::status_codes::OK, U("Classification has been computed.")); 
            return true;
        }
        else
        {
            message_reply(web::http::status_codes::InternalError, U("Classification failed for facial region " + facialRegionName + ". Corrupted model file!"));		        
            return false; 
        }        
}

PFLresult FaceScreeningObject::computePFLmeasure(const web::http::http_request& message, std::string ethnicity_code, std::string subjectGender, const float age)
{
	this->subjectGender = subjectGender;
	const auto Distance3D = [](Point3D a, Point3D b) 
		{
			return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); 
		};
	const double right_PFL = Distance3D(landmarks["right_en"], landmarks["right_ex"]);
	const double left_PFL = Distance3D(landmarks["left_en"], landmarks["left_ex"]);
	
	msPFLMeasure pfl_measure;
	const auto PFLTable = (subjectGender.compare("M") == 0) ? pfl_measure.GetMalePFLTable() : pfl_measure.GetFemalePFLTable();
	pflResult.rawPFL = (right_PFL + left_PFL) / 2.0F;
	pflResult.zScore = pfl_measure.CalculateZScore(PFLTable, age, pflResult.rawPFL);
	pflResult.percentile = pfl_measure.CalculatePercentile(pflResult.zScore);

	return pflResult;
}

void FaceScreeningObject::GeneratePartialLatexReport(const web::http::http_request& message, web::json::value reportInput, std::string tempLatexDir, std::string tempLatexFilename)
{
	// reportInput needs to be sanitized to prevent the latex compilter choking or pdfs turning out unsightly:  
	// no   # $ % & ~ _ ^ \ { }  , etc., max. length, e.g,  40 bytes
	auto ReplaceAll = [](std::string& str, const std::string& from, const std::string& to) 
	{
		size_t start_pos = 0;
		while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
			str.replace(start_pos, from.length(), to);
			start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
		}
	};

	auto latexSanitize = [&](utility::string_t input) -> utility::string_t
	{
		std::string sanitizedInput(utility::conversions::to_utf8string(input));
		ReplaceAll(sanitizedInput, "\\", "\\\\");
		ReplaceAll(sanitizedInput, "{", "\\{");
		ReplaceAll(sanitizedInput, "}", "\\}");
		ReplaceAll(sanitizedInput, "^", "\\^");
		ReplaceAll(sanitizedInput, "_", "\\_");
		ReplaceAll(sanitizedInput, "~", "\\~");
		ReplaceAll(sanitizedInput, "&", "\\&");
		ReplaceAll(sanitizedInput, "%", "\\%");
		ReplaceAll(sanitizedInput, "$", "\\$");
		ReplaceAll(sanitizedInput, "#", "\\#");
		return utility::conversions::to_string_t(sanitizedInput);
	};

	//std::filesystem::create_directory(processingToken);
	//std::filesystem::permissions(processingToken, std::filesystem::perms::owner_all, std::filesystem::perm_options::remove);

	const auto prevDir = std::filesystem::current_path();
	std::filesystem::path path{"tempLatex"}; 
	path /= tempLatexDir;
	std::filesystem::create_directories(path.parent_path()); 
	std::filesystem::create_directory(path);
	std::filesystem::current_path(path);

	ucout << "Writing FASD report to PDF" << endl;
	std::ofstream ofs(tempLatexFilename);

	// Print report title and scan metadata
	ofs << "\\documentclass{article} \n"
		<< "\\nonstopmode \n" // to keep the compiler going even if errors occur (check existence of pdf output to treat this case); NOTE: tikz may not be impressed by this
		<< "\\usepackage{graphicx}"
		<< "\\usepackage{tikz}"
		<< "\\author{FaceScreenServer}"
		<< "\\title{" << utility::conversions::to_utf8string((reportInput.has_field(U("ReportTitle")) ? latexSanitize(reportInput[U("ReportTitle")].as_string()) : U("N/A"))) << "} \n"
		<< "\\begin{document} \n"
		<< "\\maketitle \n"
		<< "Metadata: \n"
		<< "\\newline \n"
		<< "\\begin{tabular}{l|r} \n"
		<< "\\hline \n"
		<< "Patient ID  & " << utility::conversions::to_utf8string((reportInput.has_field(U("PatientID")) ? latexSanitize(reportInput[U("PatientID")].as_string()) : U("N/A"))) << "\\\\ \n"
		<< "Scan date  & " << utility::conversions::to_utf8string((reportInput.has_field(U("ScanDate")) ? latexSanitize(reportInput[U("ScanDate")].as_string()) : U("N/A"))) << "\\\\ \n"
		<< "Patient DOB  & " << utility::conversions::to_utf8string((reportInput.has_field(U("PatientDOB")) ? latexSanitize(reportInput[U("PatientDOB")].as_string()) : U("N/A"))) << "\\\\ \n"
		<< "Dx  & " << utility::conversions::to_utf8string((reportInput.has_field(U("Dx")) ? latexSanitize(reportInput[U("Dx")].as_string()) : U("N/A"))) << "\\\\ \n"
		<< "Camera system  & " << utility::conversions::to_utf8string((reportInput.has_field(U("CameraSystem")) ? latexSanitize(reportInput[U("CameraSystem")].as_string()) : U("N/A"))) << "\\\\ \n"
		<< "\\hline \n"
		<< "\\end{tabular} \n"
		<< "\\vspace{8mm}\n"
		<< "\\newline \n";

	if (surfaceMesh != nullptr && facialTexture != nullptr)
	{
		renderToJpgFile(surfaceMesh, facialTexture, "renderedFaceFrontal.jpg", this->landmarks); 
		renderToJpgFile(surfaceMesh, facialTexture, "renderedFaceProfile.jpg", this->landmarks, true);
		ofs << "\\begin{figure} \n"
			<< "  \\caption{Face rendering} \n"
			<< "  \\centering \n"
			<< "  \\includegraphics[width = 1.0\\textwidth]{renderedFaceFrontal.jpg} \n"
			<< "  \\includegraphics[width = 1.0\\textwidth]{renderedFaceProfile.jpg} \n"
			<< "\\end{figure} \n";
	}

	// Print PFL table if pflResult has been computed
	if (pflResult.rawPFL > 0.0F)
	{
		ofs << "PFL measurements: \n"
			<< "\\newline \n" 
			<< "\\begin{tabular}{l|r} \n"
			<< "\\hline \n"
			<< std::fixed << std::setprecision(3)
			<< "Raw PFL  & " << (pflResult.rawPFL > 0.0F ? std::to_string(pflResult.rawPFL) : "N/A") << "\\\\ \n"
			<< "PFL percentile  & " << (pflResult.percentile > 0.0F ? std::to_string(pflResult.percentile) : "N/A") << "\\\\ \n"
			<< "PFL zScore  & " << (pflResult.zScore > 0.0F ? std::to_string(pflResult.zScore) : "N/A") << "\\\\ \n"
			<< "\\hline \n"
			<< "\\end{tabular} \n"
			<< "\\vspace{8mm}\n"
			<< "\\newline \n";
		
		msPFLMeasure pfl_measure;
		const auto PFLTable = (subjectGender.compare("M") == 0) ? pfl_measure.GetMalePFLTable() : pfl_measure.GetFemalePFLTable();

		// Range for drawing diagram - floor and ceil to prevent ticks at fractional positions
		const auto lowestPFLvalue = floor((*std::min_element(PFLTable.begin(), PFLTable.end(), [](const auto a, const auto b) { return a.pfl < b.pfl; })).pfl);
		const auto highestPFLvalue = ceil((*std::max_element(PFLTable.begin(), PFLTable.end(), [](const auto a, const auto b) { return a.pfl < b.pfl; })).pfl);
		const auto minAge(0.0F);
		const auto maxAge(40.0F);
		ofs << "\\hline \n"
			<< "\\begin{figure} \n"
			<< "  \\caption{PFL diagram: Note, the mean PFL values are specific to Caucasian ethnicities (" << (subjectGender.compare("M") == 0 ? "male" : "female") << ").} \n"
			<< "\\begin{tikzpicture}[xscale=0.3] \n" //TODO adjust scale to age range ??
			<< "\\draw[help lines, dotted] (" << minAge << "," << lowestPFLvalue << ") grid (" << maxAge << "," << highestPFLvalue << "); \n"
			<< "\\draw[->, thick](" << minAge << ", 0) -- (" << maxAge << ", 0); \n"; //x-axis
         // << "\\node [below] at(" << -1 << ", " << 0 << ") { FAS };\n"
         // << "\\node [below] at(" << 1 << ", " << 0 << ") { Control };\n";

		for (const auto& pflAverageValue : PFLTable)
		{
			ofs << "\\draw[black](" << pflAverageValue.age << ", " << pflAverageValue.pfl << ") circle[radius = 0.05] ; \n";
		}
		float prevAge = pfl_measure.GetRunningMean(PFLTable).at(0).first;
		float prevPFL = pfl_measure.GetRunningMean(PFLTable).at(0).second;
		for (const auto& runningMean : pfl_measure.GetRunningMean(PFLTable))
		{
			ofs << "\\draw[red](" << prevAge << "," << prevPFL << ") --(" << runningMean.first << ", " << runningMean.second << "); \n";
			prevAge = runningMean.first;
			prevPFL = runningMean.second;
		}
		if (this->subjectAge >= minAge && this->subjectAge <= maxAge)
		{
			ofs << "\\draw[blue](" << this->subjectAge << ", " << pflResult.rawPFL << ") circle[radius = 0.2] ; \n";
		}
		if (this->subjectAge < minAge)
		{
			ofs << "\\node [below] at(" << 0 << ", " << -1 << ") { Subject age missing. };\n"; 
		}
		ofs << "\\draw[< ->](0, " << highestPFLvalue << ")  node [above left]{PFL [mm]} -- (0, " << lowestPFLvalue << ") -- (" << maxAge << ", " << lowestPFLvalue << ") node [below right] {Age [years]}; \n"; // axes
			// alternative axes:
			// ofs << "\\draw(0, " << lowestPFLvalue << ") --coordinate(x axis mid) (" << maxAge << ", " << lowestPFLvalue << "); \n";
			// ofs << "\\draw(0, " << lowestPFLvalue << ") --coordinate(y axis mid) (0, " << highestPFLvalue << "); \n";
		// x-ticks and y-ticks -- the arithmetics do not work with 'pt' units for drawing the ticks (see below: coordinates of x-ticks - adding, e.g, 0.01 instead of 1pt)
		ofs << "\\foreach \\x in{ " << minAge         << ", 2, ...," << maxAge << "}	     \\draw(\\x, {" << lowestPFLvalue << " + 0.01}) -- (\\x, {" << lowestPFLvalue << " - 0.03})	node[anchor = north]{ \\x }; \n";  
		ofs << "\\foreach \\y in{ " << lowestPFLvalue << ",...," << highestPFLvalue << " }   \\draw(1pt, \\y) -- (-3pt, \\y)	node[anchor = east]{ \\y };  \n"; 
		// TODO: legend
		ofs << "\\end{tikzpicture} \n"
			<< "\\end{figure} \n";
	}

	// Print classification results and plot
	if (!closestMeanClassifications.empty())
	{
		ofs << "Classification results: \n"
			<< "\\newline \n" 
			<< "\\begin{tabular}{l|r|r} \n"
			<< "Region & Mean & Stdev \\\\ \n"
			<< "\\hline \n";
		for (const auto& classificationResult : closestMeanClassifications)
		{
			ofs << classificationResult.first << " & " << classificationResult.second.mean << " & " << classificationResult.second.stdDev <<"\\\\ \n";
		}
		ofs	<< "\\hline \n"
			<< "\\end{tabular} \n"
			<< "\\vspace{8mm}\n"
			<< "\\newline \n";

		auto nClassicications = closestMeanClassifications.size();
		auto minX(-3);
		auto maxX(3);
		auto restrictRange = [](float min, float max, float value) -> float
		{
			return value < min ? min : (value > max ? max : value);
		};

		ofs << "\\hline \n"
			<< "\\begin{figure} \n"
			<< "  \\caption{Classification diagram} \n"
			<< "\\begin{tikzpicture}[scale=1] \n"
			<< "\\draw[help lines, dotted] (" << minX << ",0) grid (" << maxX - minX << "," << nClassicications << "); \n"
			<< "\\draw[->, thick](" << minX << ", 0) -- (" << maxX - minX << ", 0); \n" //x-axis
			<< "\\node [below] at(" << -1 << ", " << 0 << ") { FAS };\n"
			<< "\\node [below] at(" << 1 << ", " << 0 << ") { Control };\n";
			size_t classificationLine(0);
			for (const auto& classificationResult : closestMeanClassifications)
			{
				classificationLine++;
				ofs << "\\node [left] at(" << minX - 1 << ", " << classificationLine << ") {" << classificationResult.first << "};\n";
				ofs << "\\draw[red, thick](" << restrictRange(minX, maxX, classificationResult.second.mean) << ", " << classificationLine << ") circle[radius = 0.05] ; \n";
				ofs << "\\draw[blue](" << restrictRange(minX, maxX, classificationResult.second.mean) - classificationResult.second.stdDev << ", " << classificationLine << ") circle[radius = 0.02] ; \n";
				ofs << "\\draw[blue](" << restrictRange(minX, maxX, classificationResult.second.mean) + classificationResult.second.stdDev << ", " << classificationLine << ") circle[radius = 0.02] ; \n";
			}
		ofs << "\\end{tikzpicture} \n"
			<< "\\end{figure} \n";
	}

	// Render and include heatmap image (only if heatmap has been computed)
	if (this->heatmap != nullptr)
	{
		renderToJpgFile(this->heatmap, nullptr, "heatmap.jpg", std::map<std::string, Point3D> {} /*landmarks*/, false, true); // nullptr --> no texture, empty map --> no landmarks rendered, true --> render scale
		ofs << "\\begin{figure} \n"
			<< "  \\caption{Heatmap} \n"
			<< "  \\centering \n"
			<< "  \\includegraphics[width = 1.0\\textwidth]{heatmap.jpg} \n"
			<< "\\end{figure} \n";
	}

	ofs << "\\end{document} \n";
	ofs.close();
	/*
	cout << "PATH: " <<  path.generic_string().c_str() << endl;
	cout << "fs::path::preferred_separator: " << std::filesystem::path::preferred_separator << endl;
	cout << "EXEC CLI:  " << (std::string("pdflatex ") + path.generic_string()).c_str() << endl;	
	cout << "IN PATH: " << std::filesystem::current_path() << endl;
	*/
	std::system("pdflatex fasdReport.latex"); //using parameter -output-directory would make chdir unnecessary
	std::filesystem::current_path(prevDir);
	//std::filesystem::remove_all(path); 
}
