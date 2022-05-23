#ifndef BELLUSLANDMARKPROCESSING_H
#define BELLUSLANDMARKPROCESSING_H

#include <cpprest/json.h>
#include <cpprest/http_listener.h>

#include <vtkSmartPointer.h>
#include <vtkPolyData.h>

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
using namespace std;

namespace bellusLandmarksProcessing
{
	// Function converts yaml formatted facial landmarks file from bellus camera to json landmark format for processing by endpoint /landmarks.
	// No ear landmarks are processed
	// Returns error message if not successful.
	bool convertBellusFaceYamlToJson(const web::http::http_request& message, const std::string& bellusYmlFaceLandmarks, web::json::value& jsonLandmarks);

	// Interpolates nasion landmark point, using average of nearby landmarks from eyes and nose, projecting this to facial mesh.
	// Returns error message if not successful.
	bool interpolateNasion(const web::http::http_request& message, web::json::value& jsonLandmarks, const vtkSmartPointer<vtkPolyData>& faceMesh);

	// Function converts yaml formatted ear landmarks file from bellus camera to json landmark format for processing by endpoint /landmarks.
    // Returns error message if not successful.
	bool convertBellusEarYamlToJson(const web::http::http_request& message, const std::string& bellusYmlEarLandmarks, web::json::value& jsonLandmarks);

}


#endif // BELLUSLANDMARKPROCESSING_H 