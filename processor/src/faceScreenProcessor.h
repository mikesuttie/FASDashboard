#include <cpprest/asyncrt_utils.h> // for nonce_generator
#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>

#include "faceScreeningObject.h"

using namespace std;
using namespace web;
using namespace utility;
using namespace http;
using namespace web::http::experimental::listener;

class FaceScreenProcessor
{
public:
	FaceScreenProcessor() {}
	FaceScreenProcessor(utility::string_t url, const string_t& faceModelDBfile);

	pplx::task<void> open() { return m_listener.open(); }
	pplx::task<void> close() { return m_listener.close(); }

private:
	void handle_options(http_request request);
	void handle_get(http_request message);
	void handle_put(http_request message);
	void handle_post(http_request message);
	void handle_delete(http_request message);

	// Reads configuration values from a file named faceScreenServerConfig.json located in the same directory as server binary.
	void readFaceScreenServerConfig();
	
	// Reads config file faceScreenServerUsers.json if located in the same directory as server binary into a map of authorized users and their passwords.
	// If no such config file could be found, there are no access retrictions to issuing processing tokens.
	void readFaceScreenServerUsers();
	
	// Maps users to passwords. Empty map --> No access retrictions in place!
	map<std::string, std::string> faceScreenServerUsers; 

	//Extracts processingToken from http_message and finds faceScreeningObject from map faceScreeningObjects
	//Returns with http error response if no token in message or no faceScreeningObject with specified token exists.
	std::optional<std::shared_ptr<FaceScreeningObject>> findFaceScreeningObject(const http_request& message);

	http_listener m_listener;

	utility::nonce_generator m_processingToken_generator;

	// Unique processing token returned by server upon request by GET on /. 
	int nextProcessingToken = 0; //TODO: Permit this in debug mode only, use nonce otherwise.

	// Map: processing token to processing instance. Used to access a processing session through the REST API.
	map<utility::string_t, std::shared_ptr<FaceScreeningObject>> faceScreeningObjects; 

	// Mutex for faceScreeningObjects ... to be used when requesting new processingTokens while potentially deleting older faceScreeningObjects
	std::mutex faceScreeningObjects_mutex;
	
	// Map: reportID token to pdf file. Used for retrieving FASD reports.
	map<utility::string_t, std::string> faceScreeningPDFreports; 

	// Datastructure read from server config file (modelsDB.json), containing file paths of all models (build with potentially different landmark sets) for each ethnicity.
	web::json::value modelDescriptors; 
	
	// Datastructure read from server config file (modelsDB.json), containing a map from landmarkset type to set of landmark names this landmark set type has to contain.
	web::json::value landmarkSetTypes; 
	
	// Configured paths to split and unsplit models for currently uploaded landmarks set and ethnicity. Loaded from file modelsDB.json.
	web::json::value modelDataDirs; 

	// Root directory of face models, e.g., D:\ShapeFindvs2010\ShapeFindApplications\FaceScreen\Release\dTest
	filesystem::path m_modelsRootDirectory;

	// Limiting number of FacescreeningObjects instead of monitoring available memory. Monitoring available memory is complicated cross-platform!
	// If model files are read separately for each classification job, and not shared between faceScreeningObjects/processingTokens, ...
	// ... memory consumption may be > 200 MB per token.
	size_t max_Number_FacescreeningObjects = 3;

	// FaceScreeningObjects shall be deleted only if they are old enough.
	unsigned int min_Lifetime_in_seconds_FacescreeningObjects = 600 / 30;
};

