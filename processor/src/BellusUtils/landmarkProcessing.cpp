#include "faceScreenServerDefinitions.h"

#include <cpprest/json.h>
#include <cpprest/http_listener.h>

#include <algorithm>
#include <string>
#include <vector>

#include <vtkCellLocator.h>
#include <vtkSmartPointer.h>
#include <vtkPolyData.h>

#include "landmarkProcessing.h"

#include "../utils/yaml/Yaml.hpp"
#include "../mathUtils/geometryFunctions.h"
#include "../mathUtils/C3dVector.h"

namespace
{
	bool pickLandmarksFromYaml(const web::http::http_request& message, Yaml::Node& bellusLandmarks, web::json::value& jsonLandmarks, const std::map<size_t, std::string> bellusLandmarkIndices)
	{
		// mini-yaml does not yet support complex datatypes
		Yaml::Node& landmarkSrc = bellusLandmarks["Point3f"]["data"];
		std::string lmS = landmarkSrc.As<std::string>();
		std::stringstream lmString(lmS.substr(1, lmS.size() - 2));
		vector<float> landmarkCoords;
		while (lmString.good())
		{
			string substr;
			getline(lmString, substr, ',');
			try 
			{
				landmarkCoords.push_back(std::stof(substr)); 
			}
			catch (const std::invalid_argument& e)
			{
				message_reply(web::http::status_codes::BadRequest, U("Bellus3D landmark yaml data : float values of coordinates could not be converted - invalid argument: " 
					+utility::conversions::to_string_t(e.what())));
				return false;
			}
			catch (const std::out_of_range& e)
			{
				message_reply(web::http::status_codes::BadRequest, U("Bellus3D landmark yaml data : float values of coordinates could not be converted - out of float range: " 
					+ utility::conversions::to_string_t(e.what())));
				return false;
			}
		}

		for (const auto& lm : bellusLandmarkIndices)
		{
			const size_t lmIdx = lm.first;
			if (lmIdx * 3 + 2 > landmarkCoords.size() - 1)
			{
				message_reply(web::http::status_codes::BadRequest, U("Bellus3D landmark yaml data : float values of coordinates could not be converted: insufficient data (yaml landmarks). "));
				return false;
			}

			web::json::value lmCoords;
			try
			{
				const web::json::value xCoord(landmarkCoords[lmIdx * 3 + 0]);  
				const web::json::value yCoord(landmarkCoords[lmIdx * 3 + 1]);
				const web::json::value zCoord(landmarkCoords[lmIdx * 3 + 2]);
				lmCoords[0] = xCoord;
				lmCoords[1] = yCoord;
				lmCoords[2] = zCoord;
			}
			catch (const std::exception& e)
			{
				ucout << "Bellus landmark data: float values could not be converted: " << e.what() << std::endl;
				message_reply(web::http::status_codes::BadRequest, U("Bellus3D landmark yaml data : float values of coordinates could not be converted. "));
				return false;
			}
			jsonLandmarks[utility::conversions::to_string_t(lm.second)] = lmCoords;
		}
		return true;
	}
}

namespace bellusLandmarksProcessing
{
	bool convertBellusFaceYamlToJson(const web::http::http_request& message, const std::string& bellusLandmarksYaml, web::json::value& jsonLandmarks)
	{
		//mini-yaml does not yet support tags "!!"
		std::string bellusLandmarksYamlCp(bellusLandmarksYaml);
		std::replace(bellusLandmarksYamlCp.begin(), bellusLandmarksYamlCp.end(), '!', '#');
	
		auto ReplaceAll = [](std::string& str, const std::string& from, const std::string& to) 
		{
			size_t start_pos = 0;
			while ((start_pos = str.find(from, start_pos)) != std::string::npos) 
			{
				str.replace(start_pos, from.length(), to);
				start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
			}
		};

		ReplaceAll(bellusLandmarksYamlCp, "\r", ""); 	        // Convert line endings to Windows format ....
		ReplaceAll(bellusLandmarksYamlCp, "\n", "\r\n");	// .... to make the Yaml parser happy.	
		
		Yaml::Node root;
		try
		{
			Yaml::Parse(root, bellusLandmarksYamlCp);
		}
		catch (const std::exception& e)
		{
			ucout << "A standard exception was caught when reading Bellus3D landmark yaml data : " << e.what() << std::endl;
			message_reply(web::http::status_codes::BadRequest, U("Bellus3D landmark yaml data could not be decoded."));
			return false;
		}

		try
		{
			const auto& landmarkSrc = root["Point3f"]["data"];
		}
		catch (const std::exception& e)
		{
			ucout << "3D Landmark data not found when reading Bellus3D yaml data : " << e.what() << std::endl;
			message_reply(web::http::status_codes::BadRequest, U("Bellus3D landmark yaml data not in expected format: Node Point3f/data is missing."));
			return false;
		}

		//  Definition of landmark indices: 77-landmark active shape model; See stasm4.pdf p. 3
		const std::map<size_t, std::string> bellusLandmarkIndices =
		{
			{30, "right_en"},
			{34, "right_ex"},
			{32, "right_upper_mid"},
			{36, "right_lower_mid"},
			{44, "left_ex"},
			{40, "left_en"},
			{42, "left_upper_mid"},
			{46, "left_lower_mid"},
			{52, "pronasale"},
			{56, "subnasale"},
			{58, "right_alare"}, // addtional to 22 point model
			{54, "left_alare"}, // additional to 22 point model
			{1, "right_tragion"}, // good fit?
			{11, "left_tragion"}, // good fit?
			{63, "left_cupid"},
			{62, "lip_centre"},
			{61, "right_cupid"},
			{74, "lower_lip_centre"},
			{59, "right_ch"},
			{65, "left_ch"},
			{6, "gnathion"},  // is actual index 28  as legacy code suggests ???  --> actually not, because it would not make sense to compute 'nasion_midpoint' from this point by averaging with exactly one more point
			{28, "P1"}, // temporary landmark for nasion interpolation -- close to left_upper_mid (above eye)
			{29, "P2"} // temporary landmark for nasion interpolation -- close to right_upper_mid (above eye)
		};

		if (!pickLandmarksFromYaml(message, root, jsonLandmarks, bellusLandmarkIndices))
		{
			return false;
		}
		
		return true;
	}

	bool interpolateNasion(const web::http::http_request& message, web::json::value& jsonLandmarks, const vtkSmartPointer<vtkPolyData>& faceMesh)
	{
		try
		{
			const auto& p1json = jsonLandmarks.at(U("P1"));
			const auto& p2json = jsonLandmarks.at(U("P2"));

			double p1[] = { p1json.at(0).as_double(), p1json.at(1).as_double(), p1json.at(2).as_double() };
			double p2[] = { p2json.at(0).as_double(), p2json.at(1).as_double(), p2json.at(2).as_double() };

			double nasion_midpoint[3] = { (p2[0] + p1[0]) / 2, (p2[1] + p1[1]) / 2, (p2[2] + p1[2]) / 2 };

			double ex1[3] = { p1[0], p1[1], p1[2] };
			double ex2[3] = { p2[0], p2[1], p2[2] };
			C3dVector axes[3];;

			axes[0] = C3dVector(p1[0] - p2[0], p1[1] - p2[1], p1[2] - p2[2]);
			axes[0].Normalize();

			// ms: the second axis is approximately given by two landmarks in a similar fashion
			const auto& ganthionJson = jsonLandmarks.at(U("gnathion"));

			double v1[3] = { ganthionJson.at(0).as_double(), ganthionJson.at(1).as_double(), ganthionJson.at(2).as_double() };
			double v2[3] = { nasion_midpoint[0], nasion_midpoint[1], nasion_midpoint[2] };// { p2[0], p2[1], p2[2] };

			double closestPt1[3] = { 0, 0, 0 };
			double closestPt2[3] = { 0, 0, 0 };
			double t1(0), t2(0);

			// ms: Alter the 2nd set of axes. M.Suttie
			geom::DistanceBetweenLines(ex1, ex2, v1, v2, closestPt1, closestPt2, t1, t2);
			// ms: Alter 2nd axes x, y and z coords to reposition
			v1[0] += closestPt2[0] - closestPt1[0];
			v1[1] += closestPt2[1] - closestPt1[1];
			v1[2] += closestPt2[2] - closestPt1[2];

			v2[0] += closestPt2[0] - closestPt1[0];
			v2[1] += closestPt2[1] - closestPt1[1];
			v2[2] += closestPt2[2] - closestPt1[2];

			axes[1] = C3dVector(v1[0] - v2[0], v1[1] - v2[1], v1[2] - v2[2]);
			// ms: but we have to make them orthogonal, so the landmarks were just to get us close
			axes[1] -= axes[0] * DotProduct(axes[0], axes[1]);
			axes[1].Normalize();

			// ms: check
			float errorAxisAngle1 = DotProduct(axes[0], axes[1]); // ASSERT(error < 1e-5); 
			// ms: then the third axis is simply orthogonal to the first two			
			float errorAxisAngle2 = DotProduct(axes[0], axes[2]); // ASSERT(error < 1e-5); 
			float errorAxisAngle3 = DotProduct(axes[1], axes[2]); // ASSERT(error < 1e-5); 

			if (!(errorAxisAngle1 < 1e-5 && errorAxisAngle2 < 1e-5 && errorAxisAngle3 < 1e-5))
			{
				message_reply(web::http::status_codes::BadRequest, U("Bellus3D landmarks: Nasion could not be interpolated: Ortho-Axis construction failed. "));
				return false;
			}

			axes[2] = CrossProduct(axes[0], axes[1]);
			double projPoint[3] = { nasion_midpoint[0] - (axes[2].x * 100), nasion_midpoint[1] - (axes[2].y * 100), nasion_midpoint[2] - (axes[2].z * 100) };

			double tolerance = 0.1;
			double pLine(0);
			double intersectPoint[3] = { 0, 0, 0 };
			double pSurf[3] = { 0, 0, 0 };		
			int cellSubId; // rh: vtkIdType (typedef long long) does not match function IntersectWithLine below.

			vtkNew<vtkCellLocator> cell_loc;
			cell_loc->SetDataSet(faceMesh);
			cell_loc->BuildLocator();
			cell_loc->IntersectWithLine(nasion_midpoint, projPoint, tolerance, pLine, intersectPoint, pSurf, cellSubId);

			web::json::value lmCoords;
			lmCoords[0] = intersectPoint[0];
			lmCoords[1] = intersectPoint[1];
			lmCoords[2] = intersectPoint[2];
			jsonLandmarks[U("nasion")] = lmCoords;

		}
		catch (const web::json::json_exception& ex)
		{
			// Should not happen as long as bellusLandmarkIndices are defined properly.
			message_reply(web::http::status_codes::BadRequest, U("Bellus3D landmarks: Nasion could not be interpolated: Temp Landmarks P1,P2 missing: " 
				+ utility::conversions::to_string_t(ex.what())));
			return false;
		}

		return true;
	}

	bool convertBellusEarYamlToJson(const web::http::http_request& message, const std::string& bellusYmlEarLandmarks, web::json::value& jsonLandmarks)
	{
		//mini-yaml does not yet support tags "!!"
		std::string bellusLandmarksYamlCp(bellusYmlEarLandmarks);
		std::replace(bellusLandmarksYamlCp.begin(), bellusLandmarksYamlCp.end(), '!', '#');

		Yaml::Node root;
		try
		{
			Yaml::Parse(root, bellusLandmarksYamlCp);
		}
		catch (const std::exception& e)
		{
			ucout << "A standard exception was caught when reading Bellus3D landmark yaml data : " << e.what() << std::endl;
			message_reply(web::http::status_codes::BadRequest, U("Bellus3D landmark yaml data could not be decoded."));
			return false;
		}

		try
		{
			const auto& landmarkSrc = root["Point3f"]["data"];
		}
		catch (const std::exception& e)
		{
			ucout << "3D Landmark data not found when reading Bellus3D yaml data : " << e.what() << std::endl;
			message_reply(web::http::status_codes::BadRequest, U("Bellus3D landmark yaml data not in expected format: Node Point3f/data is missing."));
			return false;
		}

		const std::map<size_t, std::string> bellusLandmarkIndices =
		{
			{8, "lower_right_ear_attachment"},
			{14, "tragion_right"},
			{24, "lower_left_ear_attachment"},
			{30, "tragion_left"},
		};

		if (!pickLandmarksFromYaml(message, root, jsonLandmarks, bellusLandmarkIndices))
		{
			return false;
		}

		return "";
	}

} // namespace bellusLandmarksProcessing

