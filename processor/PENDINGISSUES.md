# FaceScreenServer 

This page describes open issues and lists future task for improving FaceScreenServer.
---
Please remove any solved issues from this list as part of repo merges!
---
## Shortcomings and defects to be addressed:

1. Licensing: Before publishing the codebase, proper licensing annotations should be inserted into relevant files. A=GPL is proposed as a suitable license. Note that to compy with this license, an endpoint needs to be implemented that delivers access to source code. A GH link may be sufficient.

2. Race condition in classification for facial subregions: 
Running classification in parallel (as in the javascript `Promise.All(...)` call in FaceScreenFront) produces inconsistent results over different runs. This does not happen when running sequentially.
It has been confirmed that neither the OpenMP calls, nor the final aggregation in `std::map<std::string, classificationResult> closestMeanClassifications;` are to blame.
The main suspect for data races is intermediate results in ClassificationTools objects, see ```bool FaceScreeningObject::computeClassification()```.

3. Texture rendering for FASD report: OpenGL binding errors (resulting in server crash) occur in the current implementation when using textures to render portraits. See:
```
vtkSmartPointer<vtkUnsignedCharArray> FaceScreeningObject::renderToJpg(vtkSmartPointer<vtkPolyData> graphicObject, vtkSmartPointer<vtkTexture> facialTexture, std::map<std::string, Point3D> landmarks, bool renderProfile)
{
....
	{
//		actor->SetTexture(facialTexture); TODO: check if binding texture works properly, to avoid OpenGL errors.
	}
....
}
```
Textures are currently deactivated. 
Task: Find a way to check `facialTexture` can be bound to `actor` beforehand.


4. Occassional crashes following call `tri->Update()` 
in resampling the mesh. Locations:
```
src/heatmapProcessing/vtkSurfacePCA.cpp
src/subjectClassification/classificationTools.cpp
```
Task: Introduce safeguards.

5. Occasional crashes during debug output in `src/faceScreenProcessor.cpp`:
```
ucout << message.to_string() << endl;
```
Task: Replace call of `to_string()` with extraction of data from the message and custom logging (also see logging feature below).

6. Intermittent segfaults when rendering heatmaps with mesa. Perhaps issues similar to the above rendering glitches. If not certainty can be reached towards preventing these issues, all vtk rendering etc. should run in a process seperate from faceScreenServer. 




---
---

## Features to be completed

1. Logging with a logging framework (e.g., loguru or spdlog; not glog). Output written to a file. Log entries should include timestamps and a part of the processingToken (e.g., last 4 digits).

2. Binary model files for face models to avoid the slowdown of parsing float and double values. The conversion from ASCII to binary should be part of the platform dependent deployment process to avoid data format issues.

3. Authentication: Only registered users with password should be able to obtain processing tokens. A mechanism to load a file with users and password into the server is implemented. Remaining work: transmit client auth data (base64 in message header) and decode this in the code handling endpoint /processingToken: Issue processing token only if there's a match to a std::map user entry. Reject otherwise. If the std::map containing users and pwds is empty, always issue a token, but log a warning message.

4. Uploading facial mesh: Return a metric of reprojection/resampling errors. This may help to prevent processing of unsuitable or rogue meshes.

5. When uploading landmarks (endpoint /landmarks), check landmarks coincide with facial surface. Reject and return error message if not.

6. Landmark consistency checks, e.g., left landmarks are left of right landmarks etc.

7. Using https protocol. Set up certificates for this.

8. Tool for checking modelConfig.json file: Make sure all referenced/indexed models and training.csv files can be loaded and meet expectations. Ideally, server and this tool use the same data loader for this process to ensure consistency.

9. Intercept http responses for adding CORS permission headers. This is currently solved with an ugly macro (i.e., no message interception) in `src/faceScreenServerDefinitions.h`.
Message interception should include all default messages (e.g., 500 code range) emitted by the cpprestsdk framework.

