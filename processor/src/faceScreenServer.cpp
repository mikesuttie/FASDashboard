#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#include <filesystem>

#include "faceScreenProcessor.h"

/*
#ifdef NDEBUG
	#pragma comment(lib, "cpprest_2_10")
#endif
#ifdef _DEBUG
	#pragma comment(lib, "cpprest_2_10d")
#endif
*/

using namespace web;
using namespace http;
using namespace utility;
using namespace http::experimental::listener;

std::unique_ptr<FaceScreenProcessor> fs_Processor;

void on_initialize(const string_t& address, const string_t& faceModelDBfile)
{
	uri_builder uri(address);
	uri.append_path(U("faceScreen/processor"));

	auto addr = uri.to_uri().to_string();
	fs_Processor = std::unique_ptr<FaceScreenProcessor>(new FaceScreenProcessor(addr, faceModelDBfile));
	try 
	{
		fs_Processor->open().wait();
	}
	catch (const web::http::http_exception& ex)
	{
		std::cerr << "Exception when opening http listener. Details: " << ex.what() << std::endl;
		exit(EXIT_FAILURE);
	}
	ucout << utility::string_t(U("Listening for requests at: ")) << addr << std::endl;

	return;
}

void on_shutdown()
{
	fs_Processor->close().wait();
	return;
}

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[])
#else
int main(int argc, char* argv[])
#endif
{
	utility::string_t port = U("34568");
	if (argc == 1)
	{
		std::cout << "Invocation : " << argv[0] << " [port] [path to modelDB.json file] [IP]:" << std::endl;
		std::cout << "Starting faceScreenServer at port " << port.c_str() << " assuming modelDB.json is located in current directory. "  << std::endl;
	}
	if (argc >= 2)
	{
		port = argv[1]; 
	}

	int portAsInt(0);
	try
	{
		portAsInt = std::stoi(port);
	}
	catch (const std::invalid_argument& ex)
	{
		std::cerr << "FaceScreenServer Error: Port number - argument invalid. " << ex.what() << std::endl;
		exit(EXIT_FAILURE);
	}
	catch (const std::out_of_range& ex)
	{
		std::cerr << "FaceScreenServer Error: Port number out of range. " << ex.what() << std::endl;
		exit(EXIT_FAILURE);
	}
	if (!(portAsInt > 0 && portAsInt < 65536))
	{
		std::cerr << "FaceScreenServer Error: Not a valid port number (" << portAsInt << "). Exiting ..." << std::endl;
		exit(EXIT_FAILURE);
	}

	utility::string_t faceModelDBfile = std::filesystem::path("./modelDB.json");
	if (argc >= 3)
	{
		faceModelDBfile = std::filesystem::path(argv[2]); 
	}

	if (!std::filesystem::exists(faceModelDBfile))
	{
		std::cerr << "Face model data file (modelDB.json) does not exist. Exiting ..." << std::endl;
		exit(EXIT_FAILURE);
	}

	utility::string_t address = U("http://localhost:");
	if (argc >= 4)
	{
		address = utility::conversions::to_string_t(argv[3]);
	}
	address.append(port);

	on_initialize(address, faceModelDBfile);
	std::cout << "Press ENTER to exit faceScreenServer." << std::endl;

	std::string line;
	std::getline(std::cin, line);

	on_shutdown();
	return 0;
}
