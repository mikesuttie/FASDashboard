#include "faceScreenServerDefinitions.h"

#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/filestream.h>

#include <vtkOBJReader.h>
#include <vtkImageReader2.h>
#include <vtkJPEGReader.h>
#include <vtkPNGReader.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkTexture.h>
#include <vtkUnsignedCharArray.h>

#include <array>
#include <cstdio> // C-style I/O used for temp files
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>

#include "utils/zstr/zstr.hpp"
#include "BellusUtils/landmarkProcessing.h"

#include "faceScreenProcessor.h"

using namespace std;
using namespace web;
using namespace utility;
using namespace http;
using namespace web::http::experimental::listener;

void FaceScreenProcessor::readFaceScreenServerConfig()
{	
	web::json::value v;
	try 
	{
		ifstream_t inFile("faceScreenServerConfig.json");
		stringstream_t  inStream;

		if (inFile) 
		{
			inStream << inFile.rdbuf();
			inFile.close();
			v = web::json::value::parse(inStream);
		}
		else
		{
			std::cerr << 
			"File faceScreenServerConfig.json not found. Using default values max_Number_FacescreeningObjects = " <<  max_Number_FacescreeningObjects
			<< " and min_Lifetime_in_seconds_FacescreeningObjects = " << min_Lifetime_in_seconds_FacescreeningObjects 
			<< std::endl;
		}
	}
	catch (web::json::json_exception ex) 
	{
		std::cout << "ERROR Parsing file faceScreenServerConfig.json. Invalid JSON format: ";
		std::cout << ex.what() << std::endl;
	}

	if (v.has_field(utility::string_t(U("processingTokenTimeoutInSeconds"))))
	{
		min_Lifetime_in_seconds_FacescreeningObjects = v[utility::string_t(U("processingTokenTimeoutInSeconds"))].as_integer();
	}
	else
	{
		std::cerr << "File faceScreenServerConfig.json does not contain value for processingTokenTimeoutInSeconds. Using default value: " 
		<< min_Lifetime_in_seconds_FacescreeningObjects << std::endl;
	}

	if (v.has_field(utility::string_t(U("maxNumProcessingTokens"))))
	{
		max_Number_FacescreeningObjects = v[utility::string_t(U("maxNumProcessingTokens"))].as_integer();
	}
	else
	{
		std::cerr << "File faceScreenServerConfig.json does not contain value for max_Number_FacescreeningObjects. Using default value: " 
		<< max_Number_FacescreeningObjects << std::endl;
	}
	std::cout << "FaceScreenServer configuration: maxNumProcessingTokens: " << max_Number_FacescreeningObjects 
		<< " processingTokenTimeout: " << min_Lifetime_in_seconds_FacescreeningObjects << std::endl;
}

void FaceScreenProcessor::readFaceScreenServerUsers()
{
	web::json::value v;
	try 
	{
		ifstream_t inFile("faceScreenServerUsers.json");
		stringstream_t  inStream;

		if (inFile) 
		{
			inStream << inFile.rdbuf();
			inFile.close();
			v = web::json::value::parse(inStream);
		}
		else
		{
			std::cerr << 
			" W A R N I N G:   File faceScreenServerUsers.json not found.  NO ACCESS RETRICTIONS TO ISSUEING PROCESSING TOKENS IN PLACE!" 
			<< std::endl;
			return;
		}
	}
	catch (web::json::json_exception ex) 
	{
		std::cout << "ERROR Parsing file faceScreenServerUsers.json. Invalid JSON format: ";
		std::cout << ex.what() << std::endl;
		return;
	}	
	try
	{
		auto dataObj = v.as_object();
		for(auto iter = dataObj.cbegin(); iter != dataObj.cend(); ++iter)
		{
		    const utility::string_t userName = iter->first;
		    const json::value password = iter->second;
		    faceScreenServerUsers.insert(std::pair<std::string, std::string>(utility::conversions::to_utf8string(userName), password.as_string()));
		}	
	}
	catch (web::json::json_exception ex) 
	{
		std::cout << "ERROR Parsing file faceScreenServerUsers.json. Format not recognized. ";
		std::cout << ex.what();
		return;
	}
}

FaceScreenProcessor::FaceScreenProcessor(utility::string_t url, const string_t& faceModelDBfile)
	: m_listener(url)
	, m_modelsRootDirectory(filesystem::path(faceModelDBfile).parent_path())
{
	m_listener.support(methods::OPTIONS, std::bind(&FaceScreenProcessor::handle_options, this, std::placeholders::_1));
	m_listener.support(methods::GET, std::bind(&FaceScreenProcessor::handle_get, this, std::placeholders::_1));
	m_listener.support(methods::PUT, std::bind(&FaceScreenProcessor::handle_put, this, std::placeholders::_1));
	m_listener.support(methods::POST, std::bind(&FaceScreenProcessor::handle_post, this, std::placeholders::_1));
	m_listener.support(methods::DEL, std::bind(&FaceScreenProcessor::handle_delete, this, std::placeholders::_1));

	// Load face model DB file (containing description of supported landmark sets and ethnicities.
	web::json::value v;
	try 
	{
		ifstream_t inFile(faceModelDBfile);
		stringstream_t  inStream;

		if (inFile) 
		{
			inStream << inFile.rdbuf();
			inFile.close();
			v = web::json::value::parse(inStream);
		}
		else
		{
			std::cerr << "ModelDB file cannot be read. Server will not work! " << std::endl;
			exit(EXIT_FAILURE);
		}
	}
	catch (web::json::json_exception ex) 
	{
		std::cout << "ERROR Parsing ModelDB file. Invalid JSON format: ";
		std::cout << ex.what();
		exit(EXIT_FAILURE);
	}

	if (v.has_field(utility::string_t(U("modelDescriptors"))) && v.has_field(utility::string_t(U("landmarkSetTypes"))))
	{
		modelDescriptors = v[utility::string_t(U("modelDescriptors"))];
		landmarkSetTypes = v[utility::string_t(U("landmarkSetTypes"))];
	}
	else
	{
		std::cerr << "ModelDB file invalid: modelDescriptor or landmarkSetTypes missing. Server will not work! " << std::endl;
		exit(EXIT_FAILURE);
	}
	
	nextProcessingToken ++;  // only for testing/debugging purposes
	
	readFaceScreenServerConfig();
	readFaceScreenServerUsers();
}

void FaceScreenProcessor::handle_options(http_request request)
  {
    // see 
    // https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS
    // https://stackoverflow.com/questions/38898776/add-access-control-allow-origin-on-http-listener-in-c-rest-sdk
    http_response response(status_codes::OK);
    response.headers().add(U("Allow"), U("GET, POST, OPTIONS, DEL, PUT"));
    response.headers().add(U("Access-Control-Allow-Origin"), CORS_PERMISSIONS); 
    response.headers().add(U("Access-Control-Allow-Methods"), U("GET, POST, PUT, DEL, OPTIONS"));
    response.headers().add(U("Access-Control-Allow-Headers"), U("Content-Type"));
    request.reply(response);
  }

std::optional<std::shared_ptr<FaceScreeningObject>> FaceScreenProcessor::findFaceScreeningObject(const http_request& message)
{
	const auto query = uri::split_query(uri::decode(message.relative_uri().query()));
	const auto processingTokenQueryParam = query.find(U("processingToken"));
	if (processingTokenQueryParam == query.end())
	{
		message_reply(status_codes::Forbidden, U("processingToken is a required parameter. It is missing in the query."));
		return {};
	}
	else // Check processing token is valid (i.e., is in map)
	{
		auto found = faceScreeningObjects.find(processingTokenQueryParam->second);
		if (found == faceScreeningObjects.end())
		{
			message_reply(status_codes::NotFound, U("Object with specified processing token does not exist on server."));
			return {};
		}
	}		
	return faceScreeningObjects[processingTokenQueryParam->second];
}

namespace {

std::optional<float> sanitizeSubjectAgeInput(const http_request& message, const utility::string_t& subjectAgeQueryParam)
{
	auto subjectAge(0.0F);
	try
	{
		subjectAge = std::stof(subjectAgeQueryParam);
	}
	catch (const std::out_of_range&) 
	{ 
		message_reply(status_codes::Forbidden, U("subjectAge parameter out of float range."));
		return{};
	}
	catch (const std::invalid_argument&) 
	{ 
		message_reply(status_codes::Forbidden, U("invalid: parameter subjectAge is malformed"));
		return{};
	}
	if (subjectAge < 0.0F)
	{
		message_reply(status_codes::Forbidden, U("Parameter subject_age: Unborns are not supported."));
		return{};
	}
	if (subjectAge > 100.0F)
	{
		message_reply(status_codes::Forbidden, U("Parameter subject_age: The subject does not require a diagnosis any more."));
		return{};
	}
	return std::optional<float>(subjectAge);
}

} // unnamed namespace


//void respondCORS(status_code, const http_request& message, body, CORSpermissions)

//
// A GET on endpoint / gives a processing token.
//                   /computeHeatmap Computes a heatmap or returns an error code if data is insufficient. Landmarks and obj need to be uploaded. params: processingToken, subjectAge
//                   /heatmapImage Renders heatmap/signature as jpeg and returns this as a stream.  params: processingToken
//                   /classificationRegions Returns json contraining list of all available classification regions for an ethnicity. params: none
//                   /computeClassification Computes FASD/Control classification (mean/stdev of cross validation). params: processingToken, facialRegion
//                   /classifications Returns classification results for each classification that has been computed with /computeClassifications. params: processingToken
//                   /PFLstatistics Returns PFL, PFL percentile, and zScore as json. Returns error code if uploaded landmarks are insufficient. params: processingToken, subjectAge, subjectGender
//                   /FASDreports Retrieves a generated FASD report (pdf file). params: reportID
void FaceScreenProcessor::handle_get(http_request message)
{
	ucout << "Called GET ... " << endl;
	ucout << message.to_string() << endl;

	// GET without parameters returns new processing token
	const auto paths = uri::split_path(uri::decode(message.relative_uri().path()));

	// case: return new processing token 
	if (paths.empty())
	{
		// Obsolete as of 25/11/2020. This endpoint was for testing only.
/*
		nextProcessingToken++;  // TODO Serial number should only be available in debug mode, nonce only otherwise.
		ucout << "Returning processing token " << nextProcessingToken << endl;

		const utility::string_t generatedProcessingToken = utility::conversions::to_string_t(std::to_string(nextProcessingToken));

		faceScreeningObjects[generatedProcessingToken] = std::make_shared<FaceScreeningObject>();

		message_reply(status_codes::OK, generatedProcessingToken);
*/		
//	    	http_response response(status_codes::OK);
//	        response.headers().add(U("Access-Control-Allow-Origin"), CORS_PERMISSIONS);
//	        response.set_body(generatedProcessingToken);
//	        message.reply(response);

		message_reply(status_codes::Forbidden, U("Endpoint not supported any more."));
		return;
	}

	//TODO: make sure path not too long  --> send 414 status_codes::URITooLong

	const utility::string_t path = paths[0];

	// return nonce as processingToken
	if (path.compare(U("processingToken")) == 0)
	{
		auto issueNewToken = [&message, this]()
		{
			const utility::string_t generatedProcessingToken = m_processingToken_generator.generate();
			faceScreeningObjects[generatedProcessingToken] = std::make_shared<FaceScreeningObject>(); // Should not conflict with other nonce.
			message_reply(status_codes::OK, generatedProcessingToken);
		};

		// New processingToken is issued if:
		//	- Mem available / Number of pTs does not exceed max
		//	- The oldest object can be erased (i.e., has no http_response pending), and this brings the number below max
		if (faceScreeningObjects.size() < max_Number_FacescreeningObjects)
		{
			std::lock_guard<std::mutex> guard(faceScreeningObjects_mutex);
			issueNewToken();
		}
		else
		{
			const auto& oldestFaceScreeningObject = std::min_element(faceScreeningObjects.cbegin(), faceScreeningObjects.cend(),
				[](const auto& fSO1, const auto& fSO2) {return fSO1.second->creationTimestamp <  fSO2.second->creationTimestamp;});
			const auto& oldestProcessingToken = oldestFaceScreeningObject->first;
			const auto currentPOSIXtime = utility::datetime::utc_timestamp();
			const auto oldestObjectAge = currentPOSIXtime - oldestFaceScreeningObject->second->creationTimestamp;
			if (oldestObjectAge > min_Lifetime_in_seconds_FacescreeningObjects // not using: utility::datetime::from_seconds(min_Lifetime_in_seconds_FacescreeningObjects) 
				&& !oldestFaceScreeningObject->second->busyComputing)  
			// TODO?: check second oldest (and so forth) if the oldest is still busy
			{
				// Make sure there's sufficient capacity now.
				std::lock_guard<std::mutex> guard(faceScreeningObjects_mutex);				
				const auto numberOfTokensErased = faceScreeningObjects.erase(oldestProcessingToken); 				
				if (numberOfTokensErased > 0 && faceScreeningObjects.size() < max_Number_FacescreeningObjects)
				{
					issueNewToken();
				}
				else
				{
					message_reply(status_codes::InsufficientStorage, U("FaceScreenServer out of memory. Try again later.")); // Code 507
				}
			}
			else
			{
				message_reply(status_codes::InsufficientStorage, U("FaceScreenServer out of memory. Try again later.")); // Code 507
			}
		}
		return;
	}

	if (path.compare(U("FASDreports")) == 0)
	{
		// check reportID param
		const auto query = uri::split_query(uri::decode(message.relative_uri().query()));
		const auto reportIDQueryParam = query.find(U("reportID"));
		if (reportIDQueryParam == query.end())
		{
			message_reply(status_codes::Forbidden, U("reportID is a required parameter. It is missing in the query."));
			return;
		}
		const auto reportID = reportIDQueryParam->second.c_str();

		std::cout << "Processing request for returning FASD report with reportID: " << reportID;
		std::cout << " (name of reportID file: " << faceScreeningPDFreports[reportID] << ")" << std::endl;

		// Check existence of pdf file again, as some (e.g., cron) daemon might have erased it meanwhile
		std::filesystem::path pdfFile(faceScreeningPDFreports[reportID]);
		if (!std::filesystem::exists(pdfFile))
		{
			message_reply(status_codes::NotFound, U("FASD report pdf file does not exist any more."));
			return;
		}

		concurrency::streams::fstream::open_istream(utility::conversions::to_string_t(faceScreeningPDFreports[reportID]), std::ios::in)
			.then(
				[&message](concurrency::streams::istream pdfFile) {
					if (!pdfFile.is_valid())
					{
						message_reply(status_codes::NotFound, U("FASD report does not exist any more."));
						std::cerr << "FASD report pdf: no valid stream buffer." << std::endl;
						return;
					}					
				    	http_response response(status_codes::OK);
				        response.headers().add(U("Access-Control-Allow-Origin"), CORS_PERMISSIONS);
				        response.set_body(pdfFile);
					message.reply(response).then([&message](pplx::task<void> t)
							{
								try 
								{
									t.get();
								}
								catch (...) 
								{
									message_reply(status_codes::InternalError, U("INTERNAL ERROR: Cannot stream PDF "));
								}
							});
				})
			.then([&message](pplx::task<void>t)
				{
					try 
					{
						t.get();
					}
					catch (...) 
					{
						message_reply(status_codes::InternalError, U("INTERNAL ERROR: Cannot open PDF "));
					}
				}).wait();
		return;
	}


	// Note: Reference to FaceScreeningObject only required if client request was not for a new processing token or an existing FASD report.
	const auto requestedFaceScreenObject = findFaceScreeningObject(message)->get();
	ucout << U("GET: Retrieving FaceScreeningObject: ") << (requestedFaceScreenObject ? U("Successfully retrieved facescreen object.") :
		U("Facescreen object could not be retrieved.")) << std::endl;
	if (!requestedFaceScreenObject)
	{
		message_reply(status_codes::Forbidden, U("The processing token is not valid."));
		return;
	}

	if (path.compare(U("classificationRegions")) == 0)
	{
		if (!modelDataDirs.has_field(U("splitModelsPath")))
		{
			message_reply(status_codes::NotFound, U("No (split) models for classification available for uploaded set of landmarks and provided ethnicity code."));
			return;
		}
		const std::string facialModelDataPath = utility::conversions::to_utf8string(m_modelsRootDirectory / modelDataDirs[U("splitModelsPath")].as_string());
		//If, e.g., the data dir does not exist, the below returns "null".
			
		std::vector<json::value> jsonResponses;
		for (auto& facialRegionModelPath : std::filesystem::directory_iterator(facialModelDataPath))
		{
			// FacialModelDataPath must not end with \\ or /, otherwise no directory name is returned by filename() -- > use canonical path.
			// Canocial implemented folliwing POSIX realpath: https://pubs.opengroup.org/onlinepubs/9699919799/functions/realpath.html
			jsonResponses.push_back(json::value::string(utility::conversions::to_string_t(std::filesystem::canonical(facialRegionModelPath.path()).filename())));
		}
		json::value jsonRespsonse = json::value::array(jsonResponses);
		message_reply(status_codes::OK, jsonRespsonse);
		return;
	}

	// Case: Return heatmap
	if (path.compare(U("computeHeatmap")) == 0)
	{
		ucout << U("Computing heatmap ... ");
		// 1.) Check all data is available: obj, required landmarks, age, ethnicity code. Return error if not. 
		// 2.) Invoke computation. Give feedback about progress?
		// 3.) Render image and return it.
		if (requestedFaceScreenObject->surfaceMesh == nullptr)
		{
			ucout << std::endl << U("Facial mesh has not yet been uploaded. (compute heatmap)") << std::endl;
			message_reply(status_codes::NotFound, U("Facial mesh has not yet been uploaded."));
			return;
		}

		if (requestedFaceScreenObject->landmarks.empty())
		{
			ucout << std::endl << U("Landmarks have not yet been uploaded. (compute heatmap)") << std::endl;
			message_reply(status_codes::NotFound, U("Landmarks have not yet been uploaded."));
			return;
		}

		if (requestedFaceScreenObject->ethnicityCode.empty())
		{
			ucout << std::endl << U("No ethnicity code specified. Landmark upload may have failed. (compute heatmap)") << std::endl;
			message_reply(status_codes::Forbidden, U("No ethnicity code specified. Landmark upload may have failed."));
			return;
		}
				
		const auto query = uri::split_query(uri::decode(message.relative_uri().query()));

		const auto subjectAgeQueryParam = query.find(U("subjectAge"));
		if (subjectAgeQueryParam == query.end())
		{
			message_reply(status_codes::Forbidden, U("subjectAge is a required parameter. It is missing in the query."));
			return;
		}

		const auto subjectAge = sanitizeSubjectAgeInput(message, subjectAgeQueryParam->second);		
		
		if (!subjectAge)
		{
			ucout << U("Parameter subjectAge of invalid format or range.") << std::endl;
			return;
		}
		
		if (!modelDataDirs.has_field(U("unsplitModelsPath")))
		{
			message_reply(status_codes::NotFound, U("No (unsplit) models for heatmap computation available for uploaded set of landmarks and provided ethnicity code."));
			return;
		}

		const auto facialModelDataPath = m_modelsRootDirectory / filesystem::path(modelDataDirs[U("unsplitModelsPath")].as_string());


        pplx::create_task([=]() -> void
        {
    		// Landmarks having been uploaded implies ehtnicityCode is set and valid.									
    		requestedFaceScreenObject->computeHeatmap(message, facialModelDataPath, requestedFaceScreenObject->ethnicityCode, *subjectAge);
        }).then([=](auto resultTask)
        {
    		ucout << U(" ... done (compute heatmap)!") << std::endl;
        });
        return;
	}

	if (path.compare(U("heatmapImage")) == 0)
	{
		//  1.) Check heatmap for given processingToken has been computed.
		//  2.) Render and return heatmap image.
		requestedFaceScreenObject->renderHeatmapImage(message);
		return;
	}
	
	if (path.compare(U("frontPortrait")) == 0)
	{
		requestedFaceScreenObject->renderPortraitImage(message, false);
		return;
	}
	
	if (path.compare(U("profilePortrait")) == 0)
	{
		requestedFaceScreenObject->renderPortraitImage(message, true);
		return;
	}

	
	if (path.compare(U("heatmapPolyData")) == 0)
	{
		if (requestedFaceScreenObject->heatmap == nullptr)
		{
			message_reply(web::http::status_codes::NotFound, U("Heatmap has not yet been computed."));
			return;
		}
		
		char heatmapPolyDataFileC[L_tmpnam];
		if (!std::tmpnam(heatmapPolyDataFileC)) // Defined in header <cstdio>
		{
			// std::cout << "temporary file name: " << heatmapPolyDataFileC << '\n';
			message_reply(web::http::status_codes::InternalError, U("Could not create temp file for heatmap polyData."));
			return;
		}
		
		vtkNew<vtkXMLPolyDataWriter> writer;
		writer->SetFileName(heatmapPolyDataFileC);
		writer->SetInputData(requestedFaceScreenObject->heatmap);
		writer->SetDataModeToBinary();
		writer->Write();

		std::filesystem::path heatmapPolyDataFile(heatmapPolyDataFileC);

		if (!std::filesystem::exists(heatmapPolyDataFile))
		{
			message_reply(status_codes::NotFound, U("FASD report pdf file does not exist any more."));
			return;
		}

		concurrency::streams::fstream::open_istream(utility::conversions::to_string_t(heatmapPolyDataFile), std::ios::in)
			.then(
				[&message](concurrency::streams::istream heatmapPolyDataFile) {
					if (!heatmapPolyDataFile.is_valid())
					{
						message_reply(status_codes::InternalError, U("Heatmap polydata temp file cannot be read."));
						std::cerr << "Heatmap stream data: no valid stream buffer." << std::endl;
						return;
					}					
				    	http_response response(status_codes::OK);
				        response.headers().add(U("Access-Control-Allow-Origin"), CORS_PERMISSIONS);
				        response.set_body(heatmapPolyDataFile);
					message.reply(response).then([&message](pplx::task<void> t)
							{
								try 
								{
									t.get();
								}
								catch (...) 
								{
									message_reply(status_codes::InternalError, U("INTERNAL ERROR: Cannot stream heatmap polydata "));
								}
							});
				})
			.then([&message](pplx::task<void>t)
				{
					try 
					{
						t.get();
					}
					catch (...) 
					{
						message_reply(status_codes::InternalError, U("INTERNAL ERROR: Cannot open temp heatmap polydata "));
					}
				}).wait();
		return;
	}

	if (path.compare(U("computeClassification")) == 0)
	{
		// TODO: Check that m_faceModelDataDir actually contains a valid dataset of facial models (and extract all available ethnities) 
		// Call params, e.g.: facialRegion=Face^&ethnicityCode=CAUC
		
		const auto query = uri::split_query(uri::decode(message.relative_uri().query()));
		const auto facialRegionQueryParam = query.find(U("facialRegion"));
		if (facialRegionQueryParam == query.end())
		{
			message_reply(status_codes::Forbidden, U("facialRegion is a required parameter. It is missing in the query."));
			return;
		}
		const auto facialRegionName = utility::conversions::to_utf8string(facialRegionQueryParam->second.c_str());

		if (requestedFaceScreenObject->landmarks.empty())
		{
			message_reply(status_codes::Forbidden, U("Landmarks have not yet been uploaded."));
			return;
		}

		if (requestedFaceScreenObject->ethnicityCode.empty())
		{
			message_reply(status_codes::Forbidden, U("No ethnicity code specified. Landmark upload may have failed."));
			return;
		}

		if (!modelDataDirs.has_field(U("splitModelsPath")))
		{
			message_reply(status_codes::NotFound, U("No (split) models for classification available for uploaded set of landmarks and provided ethnicity code."));
			return;
		}

		//const std::string facialModelDataPath = utility::conversions::to_utf8string(modelDataDirs[U("splitModelsPath")].as_string());
		const filesystem::path facialModelDataPath = m_modelsRootDirectory / 
                   filesystem::path(utility::conversions::to_utf8string(modelDataDirs[U("splitModelsPath")].as_string()));
		const filesystem::path facialRegionModelDataPath = facialModelDataPath / filesystem::path(facialRegionName);
		
		if (!filesystem::exists(facialRegionModelDataPath))
		{
			message_reply(status_codes::NotFound, U("Model not available for facial region ") + utility::conversions::to_string_t(facialRegionName));
			return;
		}

		pplx::create_task([=]()
		{
	    		const bool classificationSuccessful = requestedFaceScreenObject->computeClassification(message, facialRegionModelDataPath, facialRegionName);
		});
//        }).then([=](auto resultTask)
//        {
//                ....
//        });
		return;
	}

	if (path.compare(U("classifications")) == 0)
	{
		// Generate json object of requestedFaceScreenObject->closestMeanClassifications and return it in response
		json::value jsonRespsonse;
		for (const auto& classificationResult : requestedFaceScreenObject->closestMeanClassifications)
		{
			json::value jsonClassificationResult;
			jsonClassificationResult[U("mean")] = classificationResult.second.mean;
			jsonClassificationResult[U("stdError")] = classificationResult.second.stdDev;
			jsonRespsonse[utility::conversions::to_string_t(classificationResult.first)] = jsonClassificationResult;
		}		
		message_reply(status_codes::OK, jsonRespsonse);
		return;
	}

	// PFL requires params: age, gender ( and actually also ethnicity - but no ethnicity specific data is available as of July 2020)
	if (path.compare(U("PFLstatistics")) == 0)
	{
		const auto query = uri::split_query(uri::decode(message.relative_uri().query()));

		if (requestedFaceScreenObject->ethnicityCode.empty())
		{
			message_reply(status_codes::Forbidden, U("No ethnicity code specified. Landmark upload may have failed."));
			return;
		}

		// TODO: Check float conversion works and age range is valid - as above!
		const auto subjectAgeQueryParam = query.find(U("subjectAge"));
		if (subjectAgeQueryParam == query.end())
		{
			message_reply(status_codes::Forbidden, U("subjectAge is a required parameter. It is missing in the query."));
			return;
		}
		const auto subjectAge = std::stof(utility::conversions::to_utf8string(subjectAgeQueryParam->second.c_str()));

		const auto subjectGenderQueryParam = query.find(U("subjectGender"));
		if (subjectGenderQueryParam == query.end())
		{
			message_reply(status_codes::Forbidden, U("subjectGender is a required parameter. It is missing in the query."));
			return;
		}
		const auto subjectGender = utility::conversions::to_utf8string(subjectGenderQueryParam->second.c_str());

		if (requestedFaceScreenObject->landmarks.empty())
		{
			cerr << "In FaceScreeningObject::computePFLmeasure() : PFL requires landmarks to be uploaded first." << endl;
			message_reply(status_codes::NotFound, U("PFL computation requires landmarks to be uploaded first."));
			return;
		}

		if (requestedFaceScreenObject->landmarks.find("left_en") == requestedFaceScreenObject->landmarks.end() ||
			requestedFaceScreenObject->landmarks.find("left_ex") == requestedFaceScreenObject->landmarks.end() ||
			requestedFaceScreenObject->landmarks.find("right_en") == requestedFaceScreenObject->landmarks.end() ||
			requestedFaceScreenObject->landmarks.find("right_ex") == requestedFaceScreenObject->landmarks.end())
		{
			cerr << "In FaceScreeningObject::computePFLmeasure() : Not all required landmarks were uploaded." << endl;
			message_reply(status_codes::NotFound, U("Not all required landmarks were uploaded"));
			return;
		}

		// rh: As of 17 Jun 2020, this is the only function setting gender field in the faceScreeningObject. Gender is also required for report generation, but not a REST API param there! TODO check if issues could arise.
		const auto pfl = requestedFaceScreenObject->computePFLmeasure(message, requestedFaceScreenObject->ethnicityCode, subjectGender, subjectAge);

		json::value jsonPFLresult;
		jsonPFLresult[U("PFL")] = pfl.rawPFL;
		jsonPFLresult[U("PFLpercentile")] = pfl.percentile;
		jsonPFLresult[U("PFLzScore")] = pfl.zScore;

		message_reply(status_codes::OK, jsonPFLresult);
		return;
	}

	message_reply(status_codes::NotFound, U("Endpoint is not supported."));
};

// POST 
// endpoints: /landmarks - Upload landmarks as json - (TODO returns info about validity of landmark set); params: processingToken, ethnicityCode
//            /generateFASDreport - Generate FASD report (pdf format) with all data available at the server (uploaded or computed). Returns reportID string.
void FaceScreenProcessor::handle_post(http_request message)
{
	ucout << message.to_string() << endl;

	const auto requestedFaceScreenObject = findFaceScreeningObject(message)->get();
	ucout << (requestedFaceScreenObject ? U("Successfully retrieved facescreen object. (POST)") :
		U("Facescreen object could not be retrieved. (POST)")) << endl;
	if (!requestedFaceScreenObject)
	{
		return;
	}

	// Case: Accept landmarks upload
	const auto paths = uri::split_path(uri::decode(message.relative_uri().path()));
	const utility::string_t path = paths[0];
	
	if (path.compare(U("subjectAge")) == 0)
	{
		const auto query = uri::split_query(uri::decode(message.relative_uri().query()));

		const auto subjectAgeQueryParam = query.find(U("subjectAge"));
		if (subjectAgeQueryParam == query.end())
		{
			message_reply(status_codes::Forbidden, U("subjectAge is a required parameter. It is missing in the query."));
			return;
		}

		const auto subjectAge = sanitizeSubjectAgeInput(message, subjectAgeQueryParam->second);				
		if (!subjectAge)
		{
			ucout << U("Parameter subjectAge of invalid format or range.") << std::endl;
			return;
		}
		
		requestedFaceScreenObject->subjectAge = *subjectAge;
		
		message_reply(status_codes::OK, U("Parameter subjectAge updated.") ); // Second parameter is dummy to match the CORS instrumentation macro signature.
	        return;
	}
	
	if (path.compare(U("subjectEthnicity")) == 0)
	{
		const auto query = uri::split_query(uri::decode(message.relative_uri().query()));
		const auto ethnicityCodeQueryParam = query.find(U("ethnicityCode"));
		if (ethnicityCodeQueryParam == query.end())
		{
			message_reply(status_codes::Forbidden, U("ethnicityCode is a required parameter. It is missing in the query."));
			return;
		}
		
		requestedFaceScreenObject->ethnicityCode = ethnicityCodeQueryParam->second;
		message_reply(status_codes::OK, U("Parameter ethnicityCode updated.") ); // Second parameter is dummy to match the CORS instrumentation macro signature.
	        return;	
	}
	
	if (path.compare(U("landmarks")) == 0)
	{
		ucout << U("Processing landmarks upload...") << endl;
		
		// In case uploading new landmark set fails, as other computations depend on consistency.
		requestedFaceScreenObject->ethnicityCode.clear();
		requestedFaceScreenObject->landmarks.clear();

		const auto query = uri::split_query(uri::decode(message.relative_uri().query()));
		const auto ethnicityCodeQueryParam = query.find(U("ethnicityCode"));
		if (ethnicityCodeQueryParam == query.end())
		{
			message_reply(status_codes::Forbidden, U("ethnicityCode is a required parameter. It is missing in the query."));
			return;
		}

		message.extract_json().then([&message, &requestedFaceScreenObject, &ethnicityCodeQueryParam, this](pplx::task<json::value> task) {
			try
			{
				auto const& landmarks = task.get();

				//ucout << L"R: " << landmarks.serialize() << endl;

				if (!landmarks.is_null())
				{
					//// TODO check landmarks properly formed 
					const auto identifiedLandmarkSetType = requestedFaceScreenObject->parseLandmarks(landmarks, landmarkSetTypes);
					if (identifiedLandmarkSetType.empty()) 
					{
						message_reply(status_codes::NotFound, U("Provided landmark set could not be decoded to a known landmark set type."));
						std::cerr << "Parsing landmarks: No match of provided landmark set found in landmark set type database." << std::endl;
						return;
					}

					requestedFaceScreenObject->landmarkSetType = utility::conversions::to_utf8string(identifiedLandmarkSetType);

					// Warning: Heatmap computation relies on consistently set ethnicityCode.
					const auto ethnCode(ethnicityCodeQueryParam->second);
					if (!modelDescriptors.has_object_field(ethnCode))
					{
						message_reply(status_codes::NotFound, U("No models available for provided ethnicity code ") + ethnCode);
						return;
					}
					//const auto&  // with auto, compiler makes trouble below when accessing this :
					web::json::value supportedLandmarkSetsForModel = modelDescriptors[ethnCode];

					if (!supportedLandmarkSetsForModel.has_object_field(identifiedLandmarkSetType))
					{
						message_reply(status_codes::NotFound, U("No models available for uploaded set of landmarks and provided ethnicity code ") + ethnCode);
						return;
					}

					modelDataDirs = supportedLandmarkSetsForModel[identifiedLandmarkSetType]; 

					// Set ethnicity code only if landmark parsing was successful and landmark set was matched to a model.
					requestedFaceScreenObject->ethnicityCode = utility::conversions::to_utf8string(ethnicityCodeQueryParam->second.c_str());
					message_reply(status_codes::OK, identifiedLandmarkSetType);
				}
				else
				{
					message_reply(status_codes::Forbidden, U("Landmark data is empty."));
				}
			}
			catch (web::json::json_exception ex)
			{
				wcout << "ERROR Parsing JSON: " << ex.what();
				message_reply(status_codes::BadRequest, U("Landmark data: JSON parsing error."));
			}
			catch (http_exception const& e)
			{
				wcout << e.what() << endl;
				message_reply(status_codes::BadRequest, U("Landmark upload: HTTP error."));
			}
			}).wait();
			return;
	}

    // Partial FASD report: generates a report with all data currently available, e.g,. whatever has been uploaded or computed at the time of calling this. 
    // Passed by json: the following report metatdata : Camera ID, Patient ID, Scan_date, DOB, Gender, Classification result ?, 
	if (path.compare(U("generateFASDreport")) == 0)
	{
		ucout << U("Generating partial FASD report...") << endl;

		std::srand(std::time(nullptr));
		std::string tempLatexDir("latexReportDir" + std::to_string(std::rand() * std::rand()));
		const std::string tempLatexFilename("fasdReport.latex");

		message.extract_json().then([&message, &requestedFaceScreenObject, &tempLatexDir, &tempLatexFilename, this](pplx::task<json::value> task) {
			try
			{
				auto const& reportInput = task.get();
				if (!reportInput.is_null())
				{
					requestedFaceScreenObject->GeneratePartialLatexReport(message, reportInput, tempLatexDir, tempLatexFilename);

					string_t reportID = utility::conversions::to_string_t(std::to_string(nextProcessingToken));; //TODO: nonce
					std::string pdfFilePath("tempLatex/" + tempLatexDir + "/fasdReport.pdf");
					// TODO: if pdf file exists
					{
						faceScreeningPDFreports[reportID] = pdfFilePath;
						message_reply(status_codes::OK, reportID);
						return;
					}
					message_reply(status_codes::InternalError, U("INTERNAL ERROR: PDF could not be generated."));
					return;
				}
				message_reply(status_codes::Forbidden, U("Report input json could not be processed."));
				return;
			}
			catch (http_exception const& e)
			{
				wcout << e.what() << endl;
			}
			}).
			wait();
			return;
	}

	// Full FASDreport only returns OK if all data is available and classification for at least the (entire) face region has been computed.
	if (path.compare(U("fullFASDreport")) == 0)
	{
		message_reply(status_codes::Forbidden, U("Full FASD report not yet available."));
		return;
	}

	message_reply(status_codes::NotFound, U("Endpoint is not supported."));
}

//
// A PUT permits to upload data:
//
// endpoint /objFile - Upload obj file (internally loaded as vtk surface); params: processingToken
//          /textureFile - Upload face texture in png format. params: processingToken
//			/Bellus3DzipArchive - Upload a zip archive containing files head3d.obj and head3d.jpg

void FaceScreenProcessor::handle_put(http_request message)
{
	ucout << message.to_string() << endl;

	const auto requestedFaceScreenObject = findFaceScreeningObject(message)->get();
	ucout << (requestedFaceScreenObject ? U("Successfully retrieved facescreen object (PUT).") :
		U("Facescreen object could not be retrieved (PUT).")) << endl;
	if (!requestedFaceScreenObject)
	{
		return;
	}

	// Case: Accept obj file upload
	const auto paths = uri::split_path(uri::decode(message.relative_uri().path()));
	const utility::string_t path = paths[0];
	if (path.compare(U("objFile")) == 0)
	{
		ucout << U("Processing obj file upload.") << endl;
		// This creates a temporary filename to store face meshes under till we found a way to make vtk read obj directly from the stream coming through the REST api
		std::srand(std::time(nullptr));
		std::string tempObjFilename("tempObjFile" + std::to_string(std::rand() * std::rand()) + ".obj");

		message.extract_vector().then([&tempObjFilename](std::vector<unsigned char> inVec) {
			//Is there a more direct way to extract the obj data, such as: message.body(). ... ?
			ofstream fout(tempObjFilename, ios::out | ios::binary);
			fout.write(reinterpret_cast<const char*>(&inVec[0]), inVec.size() * sizeof(char));
			fout.close();

			ucout << "wrote to file obj: " << tempObjFilename.c_str() << std::endl;
		
			}).then([&requestedFaceScreenObject, &message, &tempObjFilename]() {
				//read obj file into vtk
				vtkSmartPointer<vtkOBJReader> objReader = vtkSmartPointer<vtkOBJReader>::New();

				objReader->SetFileName(tempObjFilename.c_str());
				objReader->Update();
				std::remove(tempObjFilename.c_str());

				//TODO check validity of data, return non-OK message if not valid
				try 
				{
					requestedFaceScreenObject->surfaceMesh = objReader->GetOutput();
				}
				catch (const std::exception& e) 
				{ 
					ucout << "A standard exception was caught when vtk reads obj file, with message."
						<< e.what() << endl;
					message_reply(status_codes::NotFound, U("An exception was thrown when reading obj file by vtk library."));
					return;
				}
				}).wait();
				
		if (!requestedFaceScreenObject->surfaceMesh->GetNumberOfCells() > 0)
		{
			requestedFaceScreenObject->surfaceMesh = nullptr;
			message_reply(status_codes::NotFound, U("Mesh could not be read."));			
			return;		
		}
		ucout << "VTK objReader success." << endl;
		message_reply(status_codes::OK, U("Mesh upload sucessful."));
		return;
	}

	if (path.compare(U("textureFile")) == 0)
	{
		ucout << U("Processing texture file upload.") << endl;
		
		// This creates a temporary filename to store face textures under till we found a way to make vtk read textures directly from the stream coming through the REST api

		std::srand(std::time(nullptr));
		std::string tempTextureFilename("tempFaceTextureFile" + std::to_string(std::rand() * std::rand()) + ".png");
		
		message.extract_vector().then([&tempTextureFilename](std::vector<unsigned char> inVec) {
			ofstream fout(tempTextureFilename, ios::out | ios::binary);
			fout.write(reinterpret_cast<const char*>(&inVec[0]), inVec.size() * sizeof(char));
			fout.close();

			ucout << "wrote texture to file: " << tempTextureFilename.c_str() << std::endl;
			}).then([&requestedFaceScreenObject, &message, &tempTextureFilename]() {			
				requestedFaceScreenObject->facialTexture = vtkTexture::New();
				if (!requestedFaceScreenObject->facialTexture)
				{
					ucout << U("Could not allocate texture.") << endl;
					message_reply(status_codes::InternalError, U("Could not allocate texture."));
					std::remove(tempTextureFilename.c_str());
					return;
				}
				vtkNew<vtkPNGReader> imageReaderPng;
				vtkNew<vtkJPEGReader> imageReaderJpeg;
				const auto fileIsReadablePng = imageReaderPng->CanReadFile(tempTextureFilename.c_str());
				const auto fileIsReadableJpg = imageReaderJpeg->CanReadFile(tempTextureFilename.c_str());
				if (!fileIsReadablePng && !fileIsReadableJpg)
				{
					ucout << U("Reading texture: no jpeg or png!") << endl;
					message_reply(status_codes::BadRequest, U("The image file uploaded cannot be read: no jpeg or png."));
					std::remove(tempTextureFilename.c_str());
					return;
				}
				if (fileIsReadableJpg)
				{ 
					ucout << U("Reading JPG texture ... ") << endl;
					imageReaderJpeg->SetFileName(tempTextureFilename.c_str());
					imageReaderJpeg->Update();
					requestedFaceScreenObject->facialTexture->SetInputData((vtkDataObject*)imageReaderJpeg->GetOutput());
				}
				if (fileIsReadablePng)
				{
					ucout << U("Reading PNG texture ... ") << endl;
					imageReaderPng->SetFileName(tempTextureFilename.c_str());
					imageReaderPng->Update();
					const auto textr = (vtkDataObject*)imageReaderPng->GetOutput();
					requestedFaceScreenObject->facialTexture->SetInputData(textr);
				}
				std::remove(tempTextureFilename.c_str());
			}).wait();
			//requestedFaceScreenObject->debugViz(requestedFaceScreenObject->surfaceMesh, requestedFaceScreenObject->facialTexture);
			message_reply(status_codes::OK, U("Texture successfully uploaded"));
			return;
	}

	if (path.compare(U("Bellus3DzipArchive")) == 0)
	{
		ucout << U("Processing Bellus3D data: mesh and texture upload.") << endl;

		std::srand(std::time(nullptr));
		std::string tempBellusArchiveFilename("tempBellus3DArchiveFile" + std::to_string(std::rand() * std::rand()) + ".zip");

		message.extract_vector().then([&tempBellusArchiveFilename](std::vector<unsigned char> inVec) {
			ofstream fout(tempBellusArchiveFilename, ios::out | ios::binary);
			fout.write(reinterpret_cast<const char*>(&inVec[0]), inVec.size() * sizeof(char));
			fout.close();
			}).then([&requestedFaceScreenObject, &message, &tempBellusArchiveFilename]() {
				// 1.) Unpack zip archive: Use seperate process or zlib cpp wrapper?
				// 2.) Load jpg texture
				// 3.) Load obj mesh
				// 4.) Message: confirm success or specify error

				filesystem::path tempBellus3Ddir("Bellus3DSandbox" + std::to_string(std::rand()));
				std::filesystem::create_directories(tempBellus3Ddir);
				std::string uncompressCommand("unzip -qq -o " + tempBellusArchiveFilename + " -d " + tempBellus3Ddir.generic_u8string()); // qq - very quiet, o - force overwrite
				std::system(uncompressCommand.c_str());

				filesystem::path tempBellus3DmeshFile(tempBellus3Ddir / "head3d.obj");
				filesystem::path tempBellus3DtextureFile(tempBellus3Ddir / "head3d.jpg");

				requestedFaceScreenObject->facialTexture = vtkTexture::New();
				vtkNew<vtkJPEGReader> imageReader;
				const auto fileIsReadable = imageReader->CanReadFile(tempBellus3DtextureFile.u8string().c_str());
				if (!fileIsReadable)
				{
					message_reply(status_codes::BadRequest, U("The jpeg image file uploaded with the Bellus3D archive cannot be read."));
					// TODO: need to catch ?	
				}
				else
				{
					imageReader->SetFileName(tempBellus3DtextureFile.u8string().c_str());
					imageReader->Update();
					requestedFaceScreenObject->facialTexture->SetInputData((vtkDataObject*)imageReader->GetOutput());
				}

				vtkSmartPointer<vtkOBJReader> objReader = vtkSmartPointer<vtkOBJReader>::New();
				objReader->SetFileName(tempBellus3DmeshFile.u8string().c_str());
				objReader->Update();
				try 
				{
					requestedFaceScreenObject->surfaceMesh = objReader->GetOutput();
				}
				catch (const std::exception& e) 
				{
					ucout << "A standard exception was caught when vtk reads Bellus3D obj file: "
						<< e.what() << endl;
					message_reply(status_codes::NotFound, U("An exception was thrown when reading Bellus3D mesh (obj) file by vtk library."));
				}

				std::remove(tempBellus3DmeshFile.u8string().c_str());
				std::remove(tempBellus3DtextureFile.u8string().c_str());
				std::remove(tempBellusArchiveFilename.c_str());
				std::filesystem::remove_all(tempBellus3Ddir); // dir for uncompressing
				}).wait();
				// requestedFaceScreenObject->debugViz(requestedFaceScreenObject->surfaceMesh, requestedFaceScreenObject->facialTexture);
				
				if (!requestedFaceScreenObject->surfaceMesh->GetNumberOfCells() > 0)
				{
					requestedFaceScreenObject->surfaceMesh = nullptr;
					message_reply(status_codes::NotFound, U("Could not read mesh file head3d.obj."));				
					return;
				}
				
				message_reply(status_codes::OK, U("Successfully extracted head3d.obj and head3d.jpg."));
				return;
	}

	// Convenience endpoint to convert Bellus yaml landmarks into json format.
	// Requires mesh to interpolate nasion -> processing to be done with FaceScreeningObject -> use processingToken.
	// nasion interpolation on request by param.
	if (path.compare(U("BellusFaceLandmarksToJSON")) == 0)
	{
		ucout << U("Processing Bellus3D data: Convert facial landmarks in yaml format to json where key is landmark name. Interpolate nasion landmark.") << endl;

		//message.extract_string().then([&requestedFaceScreenObject, &message](std::wstring bellusLandmarksYaml) {
		message.extract_vector().then([&requestedFaceScreenObject, &message](std::vector<unsigned char> bellusLandmarksYaml) {
			
			web::json::value jsonLandmarks;
			
			if (!bellusLandmarksProcessing::convertBellusFaceYamlToJson(message, std::string(bellusLandmarksYaml.begin(), bellusLandmarksYaml.end()), jsonLandmarks))
			{
				return;
			}
			const auto numberOfConvertedLandmarks = jsonLandmarks.size();
			if (!(numberOfConvertedLandmarks > 0))
			{
				ucout << "Bellus YAML face landmarks could not be converted to JSON." << std::endl;
				message_reply(status_codes::BadRequest, U("Bellus YAML face landmarks could not be converted to JSON."));
				return;
			}

			const auto query = uri::split_query(uri::decode(message.relative_uri().query()));
			const auto nasionInterpolateParam = query.find(U("interpolateNasion"));
			if (nasionInterpolateParam != query.end() && nasionInterpolateParam->second.compare(U("true")) == 0)
			{
				// Check if facial mesh available for interpolating nasion. If not, return.
				if (requestedFaceScreenObject->surfaceMesh == nullptr)
				{
					ucout << "Bellus3D facial landmark yaml data cannot be converted to json: Facial mesh missing for nasion interpolation." << std::endl;
					message_reply(status_codes::BadRequest, U("Bellus3D facial landmark yaml data cannot be converted to json: Facial mesh missing for nasion interpolation."));
					return;
				}
				
				if (!bellusLandmarksProcessing::interpolateNasion(message, jsonLandmarks, requestedFaceScreenObject->surfaceMesh))
				{
					ucout << "Bellus3D facial landmark yaml data: Facial mesh is available, but interpolatino of nasion landmark still failed." << std::endl;
					message_reply(status_codes::BadRequest, U("Bellus3D facial landmark yaml data: Facial mesh is available, but interpolatino of nasion landmark still failed."));
					return;
				}

				if (!(numberOfConvertedLandmarks < jsonLandmarks.size()))
				{
					message_reply(status_codes::BadRequest, U("Nasion landmark could not be interpolated."));
					return;
				}
			}

			// Erase temp. landmarks used for interpolating nasion.
			jsonLandmarks.erase(U("P1"));
			jsonLandmarks.erase(U("P2"));

			message_reply(status_codes::OK, jsonLandmarks.serialize());

			}).wait();
			return;
	}

	// only extracts the 4 ear landmarks (2 on each side) of inteterst to typical models
	if (path.compare(U("BellusEarLandmarksToJSON")) == 0)
	{
		ucout << U("Processing Bellus3D data: Convert ear landmarks in yaml format to json where key is landmark name.") << endl;

		message.extract_vector().then([&requestedFaceScreenObject, &message](std::vector<unsigned char> bellusLandmarksYaml) {

			web::json::value jsonLandmarks;
			if (!bellusLandmarksProcessing::convertBellusEarYamlToJson(message, std::string(bellusLandmarksYaml.begin(), bellusLandmarksYaml.end()), jsonLandmarks))
			{
				return;
			}
			const auto numberOfConvertedLandmarks = jsonLandmarks.size();
			if (!(numberOfConvertedLandmarks > 0))
			{
				message_reply(status_codes::BadRequest, U("Bellus YAML ear landmarks could not be converted to JSON.")); 
				return;
			}

			message_reply(status_codes::OK, jsonLandmarks.serialize());
			}).wait();
			return;
	}

	message_reply(status_codes::NotFound, U("Endpoint is not supported."));
}

void FaceScreenProcessor::handle_delete(http_request message)
{
	ucout << message.to_string() << endl;
	const auto requestedFaceScreenObject = findFaceScreeningObject(message)->get();
	ucout << (requestedFaceScreenObject ? U("Successfully retrieved facescreen object (DEL).") :
		U("Facescreen object could not be retrieved (DEL).")) << endl;
	if (!requestedFaceScreenObject)
	{
    	http_response response(status_codes::NotFound);
        response.headers().add(U("Access-Control-Allow-Origin"), CORS_PERMISSIONS);
        response.headers().add(U("Access-Control-Allow-Methods"), U("GET, POST, PUT, DEL, OPTIONS"));
        response.set_body(U("Processing token does not exist."));
        message.reply(response);
		return;
	}

	const auto paths = uri::split_path(uri::decode(message.relative_uri().path()));
	const utility::string_t path = paths[0];

	if (path.compare(U("delete")) == 0)
	{
		// erase map element
		const auto query = uri::split_query(uri::decode(message.relative_uri().query()));
		const auto processingTokenQueryParam = query.find(U("processingToken"));

		auto found = faceScreeningObjects.find(processingTokenQueryParam->second);
		if (found == faceScreeningObjects.end())
		{
			// THis should not happen, as findFaceScreeningObject would have failed already.
		}
	
		faceScreeningObjects.erase(processingTokenQueryParam->second);

    	http_response response(status_codes::OK);
        response.headers().add(U("Access-Control-Allow-Origin"), CORS_PERMISSIONS);
        response.headers().add(U("Access-Control-Allow-Methods"), U("GET, POST, PUT, DEL, OPTIONS"));
        response.set_body(U("Erased faceScreening object."));
        message.reply(response);
		return;
	}

	message_reply(status_codes::NotFound, U("Endpoint is not supported."));
}
