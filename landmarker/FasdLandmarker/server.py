import logging
import shutil 
import imageio
import numpy
import os
import json

from typing import List
from fastapi import FastAPI, UploadFile, File, HTTPException, status
from fastapi_route_logger_middleware import RouteLoggerMiddleware

from tempfile import NamedTemporaryFile, SpooledTemporaryFile
from numpyencoder import NumpyEncoder #to address: json.dump TypeError: Object of type int64 is not JSON serializable

from FasdLandmarker import FasdLandmarker
from geometryUtils import isCloseby3D

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

Iff this single route returns http code 201, the computation of 3D landmarks is deemed successful, and a landmarks table record has been created. Ownership of this record is identical to ownership of the facescan used to compute the landmarks.

The user/client has to retrieve landmarking results through a separate route.

"""

"""
Example landmarks output:

{"initialLM_DLIB":{"left_ex":[41.84096334578982,56.34583063899695,-33.000878224397404],"left_en":[17.185074289031068,55.99008074813349,-29.635677960612426],"right_ex":[-40.088419614876344,58.21058190653277,-24.666805643141828],"right_en":[-15.975495553775378,57.29143233078067,-26.31270472545584],"nasion":[1.0847439062331794,74.3049575769728,-18.18487951835937],"gnathion":[-1.6072591898949815,-55.182565519727696,-11.861159047772418],"subnasale":[1.0522443740331027,13.679176862430335,-2.6597716246515852],"left_alare":[12.247588128857677,17.57262644575232,-8.75682689551079],"right_alare":[-10.650076633789284,17.57262644575232,-8.75682689551079],"left_ch":[22.375467442919216,-12.552091492369318,-21.405698121545345],"right_ch":[-21.76209234208036,-12.51320309669621,-19.79029951332477],"left_cupid":[6.332438583210063,-1.5831096458025158,-4.170293933879744],"right_cupid":[-5.277032152675053,-1.5831096458025158,-4.170293933879744],"lower_lip_centre":[0.529293774329478,-17.99598832720225,-5.689922024095348],"lip_centre":[0.5277032152675052,-3.1662192916050316,-4.170293933879744]},"eyeLM":{"left_en":[48.71750165067741,55.18720663608927,-40.508753714120395],"left_ex ":[12.24068141584132,56.342382786167015,-25.76838708037797],"right_en":[-10.42949046437973,57.332275041733396,-24.215080575137478],"right_ex":[-10.687531099937022,53.612473437158684,-22.23463248432219]},"faceLM":{"left_en":[43.984310344195485,55.896789149799716,-36.41346948176338],"left_ex":[15.59392331718497,56.906655107729264,-29.12226690543504],"left_cupid":[-0.97412564786332,-6.607057358624955,-2.8314211163832246],"lip_centre":[6.629950070810741,-7.165593143959933,-4.423089375486066],"left_ch":[19.983767506449325,-13.296214349324877,-17.12240352797307],"nasion":[-5.654102357557793,72.44423290649688,-17.228886136730843],"gnathion":[5.599431332424607,-55.45448164858393,-10.555828213378213],"left_alare":[17.67320238815917,20.325960405313733,-10.323720905773872],"subnasale":[-16.38799932517616,12.684637234918725,-15.070254119841287],"lower_left_ear_attachment":[64.8178275050952,3.552749665181307,-97.42217469339357],"lower_lip_centre":[3.843419243194004,-20.42851814681217,-5.967023115300109],"left_tragion":[71.15194575430282,33.811137605424605,-101.31474246428877],"right_en":[-41.36829931208087,58.11970425588506,-27.56581219752241],"right_ex":[-15.539160880765301,58.27991218374206,-26.08078658164135],"right_cupid":[5.832046203256115,-4.950768711602739,-3.065685287150759],"right_ch":[-20.104493141522532,-14.622391722198218,-15.010580758167228],"right_alare":[-13.397312243421766,19.287205342874636,-7.936082122921661],"lower_right_ear_attachment":[-69.79100683608245,5.0787684042052925,-84.42950497342919],"right_tragion":[-76.20239761759127,37.50827860762766,-97.06191672257695],"pronasale":[43.39162640881252,31.793809426843318,-31.189057085860753]},"missingFaceLM":[],"portraitLM":{"right_ex":[45.05981259755589,54.07177511706706,-38.12963197365058],"right_en":[17.739431524161105,53.218294572483316,-29.635677960612426],"left_en":[-17.683607151329834,57.47172324182196,-27.968962796109246],"left_ex":[-44.34857881040276,57.653152453523596,-29.635677960612426],"left_cupid":[0.0,-4.208977496132411,-2.6597716246515852],"lip_centre":[0.0,-4.208977496132411,-2.6597716246515852],"right_cupid":[4.2216257221400415,-4.2216257221400415,-4.170293933879744],"right_ch":[21.76209234208036,-13.057255405248217,-19.79029951332477],"left_ch":[-21.56168762650564,-12.937012575903385,-15.003607800854919],"nasion":[0.0,64.68506287951693,-15.003607800854919],"gnathion":[0.0,-51.12036784218856,-8.75682689551079],"left_alare":[-17.302462258699776,25.953693388049665,-16.589345961911174],"right_alare":[17.040122614062852,21.30015326757857,-8.75682689551079],"pronasale":[0.0,24.59078666607819,10.53831874589278],"subnasale":[0.0,12.626932488397232,-2.6597716246515852],"lower_right_ear_attachment":[54.42088186111024,0.0,-41.60398959121191],"lower_left_ear_attachment":[-74.2588443277658,0.0,-91.22726387314712],"lower_lip_centre":[0.0,-16.937400778543296,-5.689922024095348],"right_tragion":[59.33906963373017,27.387262907875463,-45.12350213951778],"left_tragion":[-69.36972978077051,32.37254056435957,-52.30155956780408]},"consensus":{"right_tragion":[-76.20239761759127,37.50827860762766,-97.06191672257695],"left_tragion":[71.15194575430282,33.811137605424605,-101.31474246428877]}}
"""


def landmarkConsensusSelection(initial3DLandmarksDLIB, eyeLandmarks3D, faceLandmarks3D, portraitLandmarks3D):
    consensusLMs = {}
    suboptimalLandmarkSelections = []
    missingLandmarks = []
  
    consensusLMs["nasion"] = initial3DLandmarksDLIB["nasion"]    
    consensusLMs["subnasale"] = initial3DLandmarksDLIB["subnasale"]    
    consensusLMs["left_cupid"] = initial3DLandmarksDLIB["left_cupid"]
    consensusLMs["right_cupid"] = initial3DLandmarksDLIB["right_cupid"]
    consensusLMs["lip_centre"] = initial3DLandmarksDLIB["lip_centre"]    
    consensusLMs["lower_lip_centre"] = initial3DLandmarksDLIB["lower_lip_centre"]
    consensusLMs["gnathion"] = initial3DLandmarksDLIB["gnathion"]

    def transferIfViable(target, source, landmarkName):
        if landmarkName in source:
            target[landmarkName] = source[landmarkName]
        else:
            missingLandmarks.append(landmarkName)


    transferIfViable(consensusLMs, portraitLandmarks3D, "pronasale")
    transferIfViable(consensusLMs, portraitLandmarks3D, "left_ch")
    transferIfViable(consensusLMs, portraitLandmarks3D, "right_ch")
    #consensusLMs["pronasale"] = portraitLandmarks3D["pronasale"]                
    #consensusLMs["left_ch"] = portraitLandmarks3D["left_ch"]            
    #consensusLMs["right_ch"] = portraitLandmarks3D["right_ch"]            

    transferIfViable(consensusLMs, faceLandmarks3D, "right_tragion")
    transferIfViable(consensusLMs, faceLandmarks3D, "left_tragion")
    transferIfViable(consensusLMs, faceLandmarks3D, "left_alare")
    transferIfViable(consensusLMs, faceLandmarks3D, "right_alare")                
    transferIfViable(consensusLMs, faceLandmarks3D, "lower_left_ear_attachment")
    transferIfViable(consensusLMs, faceLandmarks3D, "lower_right_ear_attachment")
    #consensusLMs["right_tragion"] = faceLandmarks3D["right_tragion"]
    #consensusLMs["left_tragion"] = faceLandmarks3D["left_tragion"]        
    #consensusLMs["left_alare"] = faceLandmarks3D["left_alare"]
    #consensusLMs["right_alare"] = faceLandmarks3D["right_alare"]
    #consensusLMs["lower_left_ear_attachment"] = faceLandmarks3D["lower_left_ear_attachment"]    
    #consensusLMs["lower_right_ear_attachment"] = faceLandmarks3D["lower_right_ear_attachment"]    

    # Eye selection: Use eye model results, if both eye cornes of this model are close to initial dlib model (i.e., assume higher accuracy.
    # Use dlib model otherwise
    
    dlib_rightEye_DlibDetections_closeby_VGGeyeModel_detections = False     
    if "right_en" in eyeLandmarks3D and "right_ex" in eyeLandmarks3D:
        dlib_rightEye_DlibDetections_closeby_VGGeyeModel_detections = isCloseby3D(initial3DLandmarksDLIB["right_en"], eyeLandmarks3D["right_en"]) and isCloseby3D(initial3DLandmarksDLIB["right_ex"], eyeLandmarks3D["right_ex"])

    if dlib_rightEye_DlibDetections_closeby_VGGeyeModel_detections:
        consensusLMs["right_en"] = eyeLandmarks3D["right_en"]
        consensusLMs["right_ex"] = eyeLandmarks3D["right_ex"]
    else:
        consensusLMs["right_en"] = initial3DLandmarksDLIB["right_en"]
        consensusLMs["right_ex"] = initial3DLandmarksDLIB["right_ex"]
        suboptimalLandmarkSelections.append("right_en")
        suboptimalLandmarkSelections.append("right_ex")

    dlib_leftEye_DlibDetections_closeby_VGGeyeModel_detections = False
    if "left_en" in eyeLandmarks3D and "left_ex" in eyeLandmarks3D:
        dlib_leftEye_DlibDetections_closeby_VGGeyeModel_detections = isCloseby3D(initial3DLandmarksDLIB["left_en"], eyeLandmarks3D["left_en"]) and isCloseby3D(initial3DLandmarksDLIB["left_ex"], eyeLandmarks3D["left_ex"])
    
    if dlib_leftEye_DlibDetections_closeby_VGGeyeModel_detections:
        consensusLMs["left_en"] = eyeLandmarks3D["left_en"]
        consensusLMs["left_ex"] = eyeLandmarks3D["left_ex"]        
    else:
        consensusLMs["left_en"] = initial3DLandmarksDLIB["left_en"]
        consensusLMs["left_ex"] = initial3DLandmarksDLIB["left_ex"]                    
        suboptimalLandmarkSelections.append("left_en")
        suboptimalLandmarkSelections.append("left_ex")        
    
    #Provide info about origin of landmark detection, at least for both eyes individually! In the simplest case: A list (landmark names) of suboptimal LM selection choices.    
    print ('suboptimalLandmarkSelections: ',suboptimalLandmarkSelections)
    print ('missing consensus landmarks: ', missingLandmarks)

    return consensusLMs, suboptimalLandmarkSelections, missingLandmarks


def get_application():
    app = FastAPI(title="landmarker", version="0.01")

    logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.DEBUG)
    app.add_middleware(RouteLoggerMiddleware)

    #app.add_event_handler("startup", tasks.create_start_app_handler(app))
    #app.add_event_handler("shutdown", tasks.create_stop_app_handler(app))

    #app.include_router(api_router, prefix="/api")

    return app


app = get_application()

imageDimension = 256    
flm = FasdLandmarker() 
    
@app.post("/detectLandmarks/")
async def detectLandmarks(faceFiles: List[UploadFile] = File(...)):
    
    # Assert: number of files > 1
    # Assuming first file contains mesh, second file is texture

    # SpooledTemporaryFile (a file like object) needs to be written into NamedTemporaryFile, so vtk can load the mesh (vtkObjReader has only read from file interface)
    meshTmpFile = NamedTemporaryFile()
    shutil.copyfileobj(faceFiles[0].file, meshTmpFile)
    
    textureTmpFile = NamedTemporaryFile()
        
    #flm = FasdLandmarker() 
    flm.setMeshFile(meshTmpFile.name)   # absolute file path of temp file        
    f = open("testimagewrite.png", "w+b")
    f.close()
    im = numpy.asarray(imageio.imread(faceFiles[1].file, '.png'))    
    flm.setTexture(im)
    
    flm.initialiseRenderView()     

    frontalViewImageTMP = flm.renderPipeline.renderFrontalView(imageDimensions=imageDimension)
    portraitLandmarks2D = flm.computeCnnLandmarksForPortrait(frontalViewImageTMP)    
    #imageio.imwrite('portrait2D.png', frontalViewImageTMP)

    #with open('portrait2Dlms.json', 'w') as file:
    #    json.dump(portraitLandmarks2D, file, indent=4, sort_keys=True, separators=(', ', ': '), ensure_ascii=False, cls=NumpyEncoder)                      
    
    #print ("portraitLandmarks2D portraitLandmarks2D portraitLandmarks2D", portraitLandmarks2D)
    portraitLandmarks3D = flm.initial3DLandmarksFromPortrait2Dlandmarks(portraitLandmarks2D,imageDimension=imageDimension)
    
    frontalViewImage = flm.renderPipeline.renderFrontalView(imageDimensions=imageDimension)
    portraitLandmarks2D = flm.computeCnnLandmarksForPortrait(frontalViewImage)
    initial3DLandmarksFromPortraitModel = flm.initial3DLandmarksFromPortrait2Dlandmarks(portraitLandmarks2D,imageDimension=imageDimension)
          
    initial2DLandmarks, frontalViewImage = flm.computeInitial2DLandmarks() #Uses dlib 68pt model       
    if initial2DLandmarks is None:
        #print('NO 2D landmarks detected!')
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="No initial 2D landmarks detected. Cannot derive facial views.",
            headers={"WWW-Authenticate": "Bearer"},
        )
                            
    initial3DLandmarksDLIB = flm.computeInitial3DLandmarks(initial2DLandmarks)

    leftEyeView, rightEyeView, portraitView, leftSideView, rightSideView = \
            flm.render2Dsnapshots(initial3DLandmarksDLIB)

    leftEyeImage = leftEyeView['image2D']
    rightEyeImage = rightEyeView['image2D']
    portraitViewImage = portraitView['image2D']    
    leftSideViewImage = leftSideView['image2D']
    rightSideViewImage = rightSideView['image2D']
    
    leftEyeLandmarks2D, rightEyeLandmarks2D = flm.computeCnnLandmarksForEyes(
        leftEyeImage, rightEyeImage)
            
    eyeLandmarks3D = \
        flm.project2DEyeLandmarksOntoMesh(leftEyeLandmarks2D, leftEyeView, \
                                          rightEyeLandmarks2D, rightEyeView)
    
    leftFaceLandmarks2D = flm.computeCnnLandmarksForFace(leftSideViewImage, faceSide = "left")

    #imageio.imwrite('dbg_leftEye.png', leftEyeImage)
    #imageio.imwrite('dbg_rightEye.png', rightEyeImage)
    #imageio.imwrite('portrait2D_left.png', leftSideViewImage)
    with open('portrait2D_leftlms.json', 'w') as file:
        json.dump(leftFaceLandmarks2D, file, indent=4, sort_keys=True,
                  separators=(', ', ': '), ensure_ascii=False,
                  cls=NumpyEncoder)    

    rightFaceLandmarks2D = flm.computeCnnLandmarksForFace(rightSideViewImage, faceSide = "right")
    
    faceLandmarks3D, missingFaceLandmarks3D = \
        flm.project2DFaceLandmarksOntoMesh(leftFaceLandmarks2D, leftSideView, \
                                           rightFaceLandmarks2D, rightSideView)
    
    consensusSelectionLandmarks, suboptimalSelections, missingConsensusLandmarks = landmarkConsensusSelection(initial3DLandmarksDLIB, eyeLandmarks3D, faceLandmarks3D, initial3DLandmarksFromPortraitModel)
    
    result = { 
        "initialLM_DLIB" : initial3DLandmarksDLIB, 
        "eyeLM" : eyeLandmarks3D, 
        "faceLM" : faceLandmarks3D, 
        "missingFaceLM" : missingFaceLandmarks3D, 
        "portraitLM": initial3DLandmarksFromPortraitModel, 
        "consensus" : consensusSelectionLandmarks, 
        "suboptimalConsensusSelections" : suboptimalSelections, 
        "missingConsensusLandmarks" : missingConsensusLandmarks}
    
    return result
    
