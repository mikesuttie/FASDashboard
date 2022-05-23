from fastapi import FastAPI
from fastapi_route_logger_middleware import RouteLoggerMiddleware

import logging

import shutil 
from tempfile import NamedTemporaryFile, SpooledTemporaryFile

from FasdLandmarker import FasdLandmarker

"""
Interface for face landmarker:

A single route triggers computation of face landmarks.
Single parameter is the faceScan ID, referring to a DB record
in the faceSans table.

Calling the route does:
- Retrieve the facescan from the DB, and sends an http error if none is present.- Retrieves mesh and texture data from the DB
- Computes initial 2D face landmarks
- Computes 2D views of eyes and face (left, portrait, right)
- Detects landmarks in these 2D views.
- Projects detected landmarks onto 3D mesh.
- Merges detected 3D landmarks from the 5 different views according to specified strategy.
- Writes detected 3D landmarks to new record in landmarks table in DB.
- Returns initial 2D face landmarks, the 2D view (image) these correspond to, and a hint if this initial detection is deemed successful, and a hint if the 3D projection and merging is deemed successful. 

If initial 2D landmark detection is not successful, user corrected landmarks should be submitted as an additional parameter.
In this case, a different method of deriving 2D views may be used, and automatic extraction is not attempted.

Iff this single route returns 201, the computation of 3D landmarks is deemed successful, and a landmarks table record has been created. Ownership of this record is identical to ownership of the facescan used to compute the landmarks.

The user/client has to retrieve landmarking results through a separate route.

"""


"""
Useful other endpoints:
- Get VTK 3D model for visualization
"""

def get_application():
    app = FastAPI(title="landmarker", version="0.01")

    logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.DEBUG)
    app.add_middleware(RouteLoggerMiddleware)

    #app.add_event_handler("startup", tasks.create_start_app_handler(app))
    #app.add_event_handler("shutdown", tasks.create_stop_app_handler(app))

    #app.include_router(api_router, prefix="/api")

    return app


app = get_application()


@app.get("/")
async def testEP():
    print("Halllo!")
    return {"msg":"Hallo Welt!"}
    
    
@app.post("/detectLandmarks/")
async def detectLandmarks(faceFiles: List[UploadFile] = File(...)):
    
    # Assert: number of files > 1
    # Assuming first file contains mesh, second file is texture

    # SpooledTemporaryFile (a file like object) needs to be written into NamedTemporaryFile, so vtk can load the mesh (vtkObjReader has only read from file interface)
    # 
    mesh = NamedTemporaryFile()
    shutil.copyfileobj(faceFiles[0].file, mesh)
    
    texture = NamedTemporaryFile()
    shutil.copyfileobj(faceFiles[1].file, texture)
    
    flm = FasdLandmarker()
    flm.setMeshFile(mesh.name)   # absolute file path of temp file        
    flm.setTexture(texture.name) # absolute file path of temp file
    
    flm.initialiseRenderView() 
    
    initial2DLandmarks, frontalViewImage = flm.computeInitial2DLandmarks()
    
    if initial2DLandmarks is None:
        #print('NO 2D landmarks detected!')
        return {"msg":"Error: no initial landmarks detected"} #TODO error code
                    
    initial3DLandmarks = flm.computeInitial3DLandmarks(initial2DLandmarks)
    
    leftEyeView, rightEyeView, portraitView, leftSideView, rightSideView = \
            flm.render2Dsnapshots(initial3DLandmarks)

    leftEyeImage = leftEyeView['image2D']
    rightEyeImage = rightEyeView['image2D']
    portraitImage = portraitView['image2D']    
    leftSideViewImage = leftSideView['image2D']
    rightSideViewImage = rightSideView['image2D']

    leftEyeLandmarks2D, rightEyeLandmarks2D = flm.computeCnnLandmarksForEyes(
        leftEyeImage, rightEyeImage)
        
    eyeLandmarks3D = \
        flm.project2DEyeLandmarksOntoMesh(leftEyeLandmarks2D, leftEyeView, \
                                          rightEyeLandmarks2D, rightEyeView)

    leftFaceLandmarks2D = flm.computeCnnLandmarksForFace(leftSideViewImage, faceSide = "left")

    rightFaceLandmarks2D = flm.computeCnnLandmarksForFace(rightSideViewImage, faceSide = "right")
    
    faceLandmarks3D, missingFaceLandmarks3D = \
        flm.project2DFaceLandmarksOntoMesh(leftFaceLandmarks2D, leftSideView, \
                                           rightFaceLandmarks2D, rightSideView)

    return faceLandmarks3D # return missingFaceLandmarks3D as well?
                    
    #return {"filenames": [file.filename for file in faceFiles]}    
