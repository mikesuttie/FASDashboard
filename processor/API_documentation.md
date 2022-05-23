# FaceScreenServer (API) documentation

Note: `$` indicates a command line prompt.

- TODO: Document returned error codes.

## Invocation on server side:

`$ facescreeenserver [port] [path to model metadata file] [server name]:`

### Server deployment:



### Other software required on system:
- Latex compiler with packages graphicx and tkiz.
- Unzip utility: `unzip`

### Starting FaceScreenServer on EC2 instance (ubuntu):

```
$ ssh -i [pem file] ubuntu@[ec2 instance address]
$ screen
$ facescreeenserver [port] [path to model metadata file] [server name/IP]:
Leave screen: Ctrl-a-d
Can break ssh conncection now.
Reattach screen:
$ screen -r
Select screen if there is more than one.
```

## Consuming the REST API

The API is accessible at: `http://[server name/ip]:[port]/faceScreen/processor`  
The examples below assume `ip == localhost` and `port == 34568` and (for brevity) the processing token is the string `2`. The processing token is a nonce actually.  

Note: 
- Processing tokens have a timeout starting from acquisition. After this time has lapsed, the integrity of the session is not guaranteed. 
- If the maximum number of sessions has been reached, not new processing tokens are issues, unless previously acquired tokens lapse due to timeout.  

The examples demonstration consumption of the API with curl. Note that it may be necessary to escape the ampersand with a circonflexe: `^&`.  

### **GET method endpoints**

### `/processingToken`

Returns a nonce string referencing a faceScreen processing session. The returned nonce string has to be used in subsequent calls referencing the same processing session as parameter `processingToken`.

**Example:**

`$ curl -X GET http://localhost:34568/faceScreen/processor/processingToken`

### `/computeHeatmap`

Computes a heatmap or returns an error code if data on server is insufficient.
Face mesh (obj) and landmarks need to be uploaded prior to invoking heatmap computation.  

**Parameters:** `processingToken`, `subjectAge`

**Example:**

`$ curl -X GET http://localhost:34568/faceScreen/processor/computeHeatmap?processingToken=2&subjectAge=12`

Returns OK/200 if the heatmap was calculated successfully.  

Returns an error code and message if no obj data has been uploaded yet, no suitable set of
landmarks has been uploaded, subjectAge is not a valid float value between 0.0 and 100.0, or
`ethnicityCode` is not supported (i.e., there is no model on the server corresponding to the specified ethnicity code).  

### `/heatmapImage`

Renders heatmap (a.k.a. signature) as jpeg and returns this as an octet stream.  

**Parameters:** `processingToken`

**Example:**

`$ curl -X GET --output testHeatmap.jpg http://localhost:34568/faceScreen/processor/heatmapImage?processingToken=2`

In this example, the jpeg-coded stream of image data is written to file testHeatmap.jpg.
If no heatmap has been computed yet, an error code and message is returned.  

### `/classificationRegions`

Returns a json array containing a list of all available classification regions for an ethnicity. Landmarks need to be uploaded first, as this sets the ethnicityCode server side.  

**Parameters:** None

**Example:**

`$ curl -X GET --output availableClassificationRegions.json http://localhost:34568/faceScreen/processor/classificationRegions`

Depending on ethnicityCode, the call may return a json-coded stream such as:   `["Eyes","Face","Malar","Nose","Philtrum"]`

The example above writes this stream into a file called `availableClassificationRegions.json`.

### `/computeClassification`

Computes FASD/Control classification for specified facial region, that is, mean and standard deviation of a cross validation style classification into classes -1 (= FASD) and 1 (= Control/non-FASD).  
Landmarks need to be uploaded prior to consuming `/computeClassfications`, which implicitly specifyies the ethnicityCode.  

**Parameters:** `processingToken`, `facialRegion`

**Example:**

`$ curl -X GET http://localhost:34568/faceScreen/processor/computeClassification?processingToken=2&facialRegion=Face`

This example computes the classification for the entire face (`facialRegion=Face`). The model for for Caucasian ethnicity may have been set when uploading landmarks to endpoint `/landmarks`.  

This git commit --amend --no-edit call can be executed several times for different facial regions. Results are accumulated server-side. All computed results are aggregated in the FASD report (see below).  

A second call, using a model restricted to the nose, may look like this:  
`$ curl -X GET http://localhost:34568/faceScreen/processor/computeClassification?processingToken=2&facialRegion=Nose`

The method returns OK/200 if the computation was successful, and an error code with string error message payload otherwise.  

### `/classifications`

Returns json-coded classification results for each classification that has been computed.  

**Parameters:** `processingToken`

**Example:**

`$ curl -X GET --output classifications.json http://localhost:34568/faceScreen/processor/classifications?processingToken=2`

If called following the example provided with the endpoint documentation of `/computeClassification` (above), the returned value may be:  

`{"Face":{"mean":0.6981015205383301,"stdError":0.12628942728042603},"Nose":{"mean":-0.483062744140625,"stdError":0.38893651962280273}}`

If no classifications have been computed yet, the method returns `"null"`.  

### `/PFLstatistics`

Returns PFL, PFL percentile, and zScore as json. Returns an error code if have landmarks not yet been uploaded or uploaded landmarks are insufficient. At least the 4 eye landmarks `left_en`, `left_ex`, `right_en`, `right_ex` have to reside on the server.  

**Parameters:** `processingToken`, `subjectAge`, `subjectGender`

**Example:**

`curl -X GET --output PFLanalysis.json http://localhost:34568/faceScreen/processor/PFLstatistics?processingToken=2&subjectAge=12&subjectGender=F`

The call may return (and write to file called `PFLanalysis.json`) a json object like this:  

`{"PFL":26.200569152832031,"PFLpercentile":51.268230438232422,"PFLzScore":0.03179515153169632}`

### `/FASDreports`

Retrieves a generated FASD report (pdf file).  

**Parameters:** `reportID`

**Example:**

`$ curl --output FASDreport.pdf -X GET http://localhost:34568/faceScreen/processor/FASDreports?reportID=2`

Saves the report accessible with `reportID=2` into file `FASDreport.pdf`.  
The parameter `reportID` is return by a prior successful call to endpoint `/generateFASDreport` (see POST method below).  



### **POST method endpoints**

### `/landmarks`

Upload json formatted landmarks. Returns a string describing identified landmarking model.  

**Parameters:** `processingToken`, `ethnicityCode`  

As an example, the landmarks to be uploaded should be formatted as follows:  

```
{
"right_upper_mid": [-89.94558, 38.707432, 7.9178],
"right_ex": [-101.226364, 28.183878, 1.975529],
"right_lower_mid": [-87.83255, 28.035084, 9.120646],
"right_en": [-75.470695, 32.86927, 6.373322],
"left_en": [-43.593288, 35.390598, 2.126038],
"left_upper_mid": [-31.203552, 43.261288, 0.36318],
"left_ex": [-19.803116, 34.989964, -7.993075],
"left_lower_mid": [-30.188482, 32.758667, 1.632968],
"left_cupid": [-46.893089, -2.576849, 31.667639],
"lip_centre": [-51.66433, -4.535646, 33.491817],
"right_cupid": [-56.881641, -4.324551, 33.613564],
"right_ch": [-73.091469, -18.181156, 25.571007],
"left_ch": [-32.768055, -14.238047, 19.614464],
"nasion": [-60.026028, 48.160839, 11.04115],
"gnathion": [-48.822662, -45.783016, 31.107973],
"left_alare": [-39.903473, 12.338382, 21.449482],
"right_alare": [-69.479446, 9.274796, 24.992481],
"pronasale": [-53.927311, 17.317726, 33.227749],
"subnasale": [-53.308018, 6.114594, 28.23843],
"lower_right_ear_attachment": [-117.178177, -25.509789, -36.058727],
"lower_left_ear_attachment": [-6.916459, -16.375231, -52.09692],
"lower_lip_centre": [-49.773396, -17.099918, 36.040085],
"tragion_right": [-127.420647, -5.983522, -46.445354],
"tragion_left": [-3.344804, 4.055732, -60.610046]
}
```

**Example:**

` $curl -H "Content-Type: application/json" --data @JWMlandmarks.json http://localhost:34568/faceScreen/processor/landmarks?processingToken=2&ethnicityCode=CAUC`

Uploads JSON formatted landmarks stored in file `JWMlandmarks.json` to processing
session associated with processingToken `2`. Ethnicity code `CAUC` for Caucasian is
specified.  

### `/generateFASDreport`

Generate FASD report (in pdf format) with all data available at the server (uploaded or
computed) and json-coded input (supplied by client) to be included in the report. Returns
'reportID' string on success, which is to be consumed by endpoint `/FASDreports` to download the pdf stream.  

**Parameters:** `processingToken`

**Example:**

`$ curl -H "Content-Type: application/json" --data @reportInput.json http://localhost:34568/faceScreen/processor/generateFASDreport?processingToken=2`

In this example, a file `reportInput.json` that is submitted to the server to inject subject master data into the report may look like this:  

`{"ReportTitle":"FaceScreen FASDreport","CameraSystem":"CanfieldStatic","PatientID":"12345678","ScanDate":"11-Nov-2019","PatientDOB":"4-Sept-2009","Dx":"fas_pfas"}`


### **PUT method endpoints**

### `/objFile`

Upload an obj file describing the facial geometry/try.  

**Parameters:** `processingToken`

**Example:**

`$ curl -T JWMmesh.obj http://localhost:34568/faceScreen/processor/objFile?processingToken=2`

This example uploads contents of Wavefront 3D-formatted file `JWMmesh.obj` to the server.
Returns OK/201 if the file could be read and converted to a vtkSurface internally.  


### `/textureFile`

Upload face texture in png or jpg format.  

**Parameters:** `processingToken`

**Example:**

`$ curl -T JWMtexture.png http://localhost:34568/faceScreen/processor/textureFile?processingToken=2`

This sends contents of png-formatted file `JWMtexture.png` to the server.  
Returns OK/200 if the file could be read and stored internally as a vtkTexture.


### `/Bellus3DzipArchive`

A convenience endpoint that can be used to upload Bellus3D archive rather than uploading
mesh and texture separately.  
Uploads Bellus3D zip archive mesh_18_41_00Bellus3D.zip. It is expected that this zip archive
contains a mesh file called “head3d.obj” and texture file called “head3d.jpg”, as per Bellus convention.  

Landmarks have to be uploaded separately afterwards, together with ethnicity code.  

**Parameters:** `processingToken`

**Example:**

`$ curl -T mesh_18_41_00Bellus3D.zip http://localhost:34568/faceScreen/processor/Bellus3DzipArchive?processingToken=2`

Uploads file `mesh_18_41_00Bellus3D.zip`.  
Returns OK/200 if the mesh could be read and converted to a vtk object, and the texture file could be read and converted into a `vtkTeture`.  


### `/BellusFaceLandmarksToJSON`

Convenience endpoint that accepts the Bellus facial landmarks file in yml format and returns
JSON formatted landmarks expected by endpoint `/landmarks`.  

**Parameters:** `interpolateNasion` (optional) – If set to `true`, nasion landmark (which is not provided by STASM software used in the Bellus3D product) will be interpolated. Requires
prior mesh upload, e.g., through endpoint `/Bellus3DzipArchive`.  

**Example:**

`$ curl -T faceLandmarks.yml http://localhost:34568/faceScreen/processor/BellusFaceLandmarksToJSON?processingToken=2^&interpolateNasion=true`


### `/BellusEarLandmarksToJSON`

Convenience endpoint that accepts the Bellus ear landmarks file in yml format and returns JSON
formatted landmarks (4 in total – 2 for each ear) expected by endpoint `/landmarks`.  
These ear landmarks should be merged (union) with the face landmarks obtained as a successful response to calling endpoint `/BellusFaceLandmarksToJSON`.  

**Parameters:** `processingToken`

**Example:**

`$ curl -T earLandmarks.yml http://localhost:34568/faceScreen/processor/BellusEarLandmarksToJSON?processingToken=2`



## A typical scenario of consuming the API - indicating suitable order of calls

```
$ curl -X GET http://localhost:34568/faceScreen/processor/processingToken
$ curl -T JWM6314_5-MAR-2014.obj http://localhost:34568/faceScreen/processor/objFile?processingToken=2
$ curl -T JWM6314_5-MAR-2014.png http://localhost:34568/faceScreen/processor/textureFile?processingToken=2
$ curl -H "Content-Type: application/json" --data @JWM6314_5-MAR-2014.json http://localhost:34568/faceScreen/processor/landmarks?processingToken=2&ethnicityCode=CAUC
$ curl -X GET http://localhost:34568/faceScreen/processor/computeHeatmap?processingToken=2&subjectAge=12
$ curl -X GET --output availableClassificationRegions.json http://localhost:34568/faceScreen/processor/classificationRegions?processingToken=2
$ curl -X GET http://localhost:34568/faceScreen/processor/computeClassification?processingToken=2&facialRegion=Face
$ curl -X GET http://localhost:34568/faceScreen/processor/computeClassification?processingToken=2&facialRegion=Nose
$ curl -X GET --output classifications.json http://localhost:34568/faceScreen/processor/classifications?processingToken=2
$ curl -X GET --output PFLananlysis.json http://localhost:34568/faceScreen/processor/PFLstatistics?processingToken=2&subjectAge=12^&subjectGender=F
$ curl -H "Content-Type: application/json" --data @reportInput.json http://localhost:34568/faceScreen/processor/generateFASDreport?processingToken=2
$ curl --output FASDreport.pdf -X GET http://localhost:34568/faceScreen/processor/FASDreports?reportID=2
```

### For processing Bellus3D data, the API may be consumed as follows:

```
$ curl -X GET http://localhost:34568/faceScreen/processor/processingToken
$ curl -T mesh_18_41_00Bellus3D.zip http://localhost:34568/faceScreen/processor/Bellus3DzipArchive?processingToken=2
$ curl -T faceLandmarks.yml http://localhost:34568/faceScreen/processor/BellusFaceLandmarksToJSON?processingToken=2&interpolateNasion=true
$ curl -T earLandmarks.yml http://localhost:34568/faceScreen/processor/BellusEarLandmarksToJSON?processingToken=2

#combine JSON data from previous two calls (union)] ....
# ... assuming all landmarks (face and ear) are stored in a file headLandmarks.json, continue as above with landmark upload and processing:

$ curl -H "Content-Type: application/json" --data @headLandmarks.json http://localhost:34568/faceScreen/processor/landmarks?processingToken=2&ethnicityCode=CAUC

$ ...

```


