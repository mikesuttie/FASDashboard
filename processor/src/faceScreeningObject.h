#ifndef FACESCREENINGOBJECT_H
#define FACESCREENINGOBJECT_H

#include <filesystem>
#include <string>
#include <map>

#include <cpprest/asyncrt_utils.h>
#include <cpprest/json.h>
#include <cpprest/http_listener.h>

#include <vtkPolyData.h>
#include <vtkTexture.h>
#include <vtkUnsignedCharArray.h>

#include "mathUtils/Point_3D.h"
#include "subjectClassification/classificationTools.h"

struct classificationResult
{
	float mean;
	float stdDev;
};

struct PFLresult
{
	float rawPFL = 0.0F; // metric absolute length
	float percentile = 0.0F;
	float zScore = 0.0F;
};

// Struct for processing an individual subject - structure used for processing data corresponding to a REST API processing token
struct FaceScreeningObject
{
	FaceScreeningObject();
	
	// Helper function to inspect intermediate result during development process.
	// Not used in production systems.
	void debugViz(vtkSmartPointer<vtkPolyData> mesh, vtkSmartPointer<vtkTexture> facialTexture);

	// Decodes json coded landmarks (received by query) into std::map of landmarks.
	// Stores a copy of the above in VTK compatible format (member landmarks_InVTKFormat).
	// Returns the type of landmark set identified from the modelDB server config file.
	utility::string_t parseLandmarks(const web::json::value& landmarksASjson, const web::json::value& landmarksSetTypesFromModelDB);

	// Selects (server-side) model and projection file and computes heatmap (a.k.a. facial signature).
	void computeHeatmap(const web::http::http_request& message, const std::filesystem::path modelFilesRootDir, const std::string ethnicity_code, const float subject_age);

	// Produces an image of the computed heatmap with color scale and sends it back to client jpeg coded via http_response.
	void renderHeatmapImage(const web::http::http_request& message);
	
	// Produces an image of the face frontal view with texture and sends it back to client jpeg coded via http_response.
	void renderPortraitImage(const web::http::http_request& message, bool renderProfile);
	
	// Computes FASD classification (mean, stdev) as class posterior (-1 control, 1 FASD), selecting model for facial subregion located at facialRegionModelDataPath
	// Returns true if classification was successful for specified facial region, false otherwise.
	bool computeClassification(const web::http::http_request& message, std::filesystem::path facialRegionModelDataPath, std::string facial_Region);

	// Computes and stores PFL, percentile, and zScore.
	PFLresult computePFLmeasure(const web::http::http_request& message, std::string ethnicity_code, std::string subjectGender, const float age);

	// Generates FASD report with data available at time of calling and compiles it into a pdf file. Sends a http response with token for download of the generated pdf.
    // Parameters: 
    // reportInput - Table of values describing subject and imaging modality to go into report.
	void GeneratePartialLatexReport(const web::http::http_request& message, web::json::value reportInput, std::string tempLatexDir, std::string tempLatexFilename);

	// Creation timestamp used for automatically deleting oldest objects upon request of new processing token (if memory is low)
	utility::datetime::interval_type creationTimestamp; // Unix/POSIX time

	// FaceScreening object shall be deleted only while not running a job for a client.
	bool busyComputing; 

	// Ethnicity code submitted through REST API.
	std::string ethnicityCode;

	// Name describing type of landmark set (e.g., manual24, Bellus3D etc.) determined when parsing client submitted set of landmarks. Return value taken from definition in modelDB.json.
	std::string landmarkSetType;

	// Facial mesh of subject submitted by client.
	vtkSmartPointer<vtkPolyData> surfaceMesh = nullptr;
	
	// Texture of subject face submitted by client.
	vtkSmartPointer<vtkTexture> facialTexture = nullptr;
	
	// Facial landmarks submitted by client.
	std::map<std::string, Point3D> landmarks;

	// Classification results (mean, stdev) for each computed facial region.
	std::map<std::string, classificationResult> closestMeanClassifications;

	// 3D Heatmap, not nullptr if computed
	vtkSmartPointer<vtkPolyData> heatmap = nullptr;

	// Subject age, not of invalid value if set by client TODO: consider nullopt value for unset age.
	float subjectAge{ -9999.0F };

private:

	// Render to jpeg and write to file a textured 3D mesh (face) with 3D points on surface displayed as speheres (landmarks).
	// Used for creating heatmap or facial renderings prior to FASD report generation with latex.
	// Parameters:
	// graphicObject - Face mesh.
	// facialTexture - Texture (color/RGB channel) for face mesh.
	// tmpImageFilename - Filename used for storing image.
	// landmarks - Names and locations of landmarks to be rendered.
	// renderProfile - Renders profile of face if true, frontal view otherwise.
	// renderScale - Includes a scale of heatmap values to the rendered image if true, does not include this scale otherwise.
	void renderToJpgFile(vtkSmartPointer<vtkPolyData> graphicObject, vtkSmartPointer<vtkTexture> facialTexture,  std::string tmpImageFilename, std::map<std::string, Point3D> landmarks, bool renderProfile = false, bool renderScale = false);
	
	// Same as above, but in-memory image generation. Intended for returning image through REST API.
	vtkSmartPointer<vtkUnsignedCharArray> renderToJpg(vtkSmartPointer<vtkPolyData> graphicObject, vtkSmartPointer<vtkTexture> facialTexture,  std::map<std::string, Point3D> landmarks, bool renderProfile = false);

	// Create in-memory jpeg coded image from vtkPolyData. Used, e.g., to render jpeg image from 3D heatmap.
	vtkSmartPointer<vtkUnsignedCharArray> renderToJpg(vtkSmartPointer<vtkPolyData> surface);

	// Heatmap image, not nullptr if rendered from 3D heatmap
	vtkSmartPointer<vtkUnsignedCharArray> jpgHeatmapImage = nullptr;

	// Vector of landmark names in order required to create VTK landmarks vector (landmarks_InVTKFormat)
	std::vector<std::string> orderedLandmarkNames;
	vtkSmartPointer<vtkPolyData> landmarks_InVTKFormat = nullptr; // different format of landmark, used in heatmap computations

	std::string subjectGender;

	// PFL result, not null if computed
	PFLresult pflResult;
	
//	std::map<std::string, std::unique_ptr<ClassificationTools>> closestMeanClassifiers;
	std::map<std::string, ClassificationTools> closestMeanClassifiers;
	std::map<std::string, float> closestMeans;
	std::map<std::string, float> closestStdDevs;	

};

#endif // FACESCREENINGOBJECT_H
