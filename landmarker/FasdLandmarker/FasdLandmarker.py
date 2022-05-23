import numpy as np
import os.path #TODO remove if need to process files dropped

import utils
import geometryUtils
import GenericLandmarker
import CNN2dLandmarker
from FaceRenderer import FaceRenderPipeline

class FasdLandmarker:
    
    def __init__(self):
        self.dlibLandmarkPredictor = GenericLandmarker.get_Dlib_68pt_faceLandmarkModel()
        self.renderPipeline = FaceRenderPipeline()
        self.cnnLandmarker = CNN2dLandmarker.CNN2dLandmarker()
        self.reset()
        
        
    def setMeshFile(self, meshFilePath):
        print('Loading mesh file: ', meshFilePath)
        self.renderPipeline.loadMeshFromObjFile(str(meshFilePath))
    
    
    def setMesh(self, mesh=None):  
       
        #TODO write mesh to file, e.g., use 
        # https://docs.python.org/dev/library/tempfile.html#tempfile.NamedTemporaryFile  ?
        if mesh is None:
            meshFilePath = os.path.join("data","subjects", "SMS64.obj")# JWM5671.obj") # TODO
            meshFilePath = os.path.join("data","subjects", "JWM5671.obj") 
            if not os.path.exists(meshFilePath):
                raise ValueError 
            self.renderPipeline.loadMeshFromObjFile(meshFilePath)    
            print('LOADED mesh at', meshFilePath)               
              
    def setTexture(self, texture): #texture: RGB image as numpy array

        #TODO write texture to file?
        #textureFilePath = '' # TODO ... 
        if texture is None:
            textureFilePath = os.path.join("data","subjects", "JWM5671.png") 
            print('Loading texture file ' , textureFilePath)
            if not os.path.exists(textureFilePath):
                raise ValueError 
            self.renderPipeline.loadTextureFromPngFile(textureFilePath)            
            
        else: # NP array expected
            print('LOADING TEXTURE FROM NUMPY ARRAY')
            ImageData = utils.numpyToVtkImageData(texture)            
            self.renderPipeline.texture.SetInputDataObject(ImageData)

    def initialiseRenderView(self):
        pass
        #self.renderPipeline.setCameraCoords(np.zeros(3))
        #self.renderPipeline.setFocalPoint(np.zeros(3))

      
    # Computes generic landmarks, using a 68point dlib model by default.
    def computeInitial2DLandmarks(self):
        frontalViewImage = self.renderPipeline.renderFrontalView()
        if frontalViewImage is not None:
            detectedLandmarks = GenericLandmarker.predictGenericLandmarks(\
                self.dlibLandmarkPredictor, frontalViewImage)
            self.dlib68LandmarkVisualisation = GenericLandmarker.drawLandmarksIntoImage(\
                frontalViewImage,detectedLandmarks)
            return detectedLandmarks, frontalViewImage
        else:
            print('N O   F R O N T A L  V I E W    R E N D E R E D!!!!')
            return



    # Projects initial 2D landmarks onto 3D mesh. 
    # Initial 3D landmarks used for taking 2D projections of areas of interest.
    def computeInitial3DLandmarks(self, initial2DLandmarks, imageDimension=512):  
        #TODO set camera position, focalpoint and upvector same as in fct computeInitial2DLandmarks
        landmarkList3D = {}
        #self.renderPipeline.iren.SetRenderWindow(self.renderPipeline.renderWindow)
        picker = self.renderPipeline.renderer.GetRenderWindow().GetInteractor().GetPicker()

        for landmark in initial2DLandmarks:
            picker.Pick(int(initial2DLandmarks[landmark][0]), 
                        imageDimension - int(initial2DLandmarks[landmark][1]), 
                        0,  
                        self.renderPipeline.renderer)
            landmarkList3D[landmark] = picker.GetPickPosition()

        return landmarkList3D
        
        
    def initial3DLandmarksFromPortrait2Dlandmarks(self, portraitLandmarks2D, imageDimension=512):
        landmarkList3D = {}

        picker = self.renderPipeline.renderer.GetRenderWindow().GetInteractor().GetPicker()

        for landmark in portraitLandmarks2D:
            picker.Pick(int(portraitLandmarks2D[landmark][0]), 
                        imageDimension - int(portraitLandmarks2D[landmark][1]), 
                        0,  
                        self.renderPipeline.renderer)
            landmarkList3D[landmark] = picker.GetPickPosition()

        return landmarkList3D
    
    def computeCnnLandmarksForPortrait(self, imagePortrait):
        return self.cnnLandmarker.predictPortraitLandmarks(imagePortrait)
        
    def computeCnnLandmarksForEyes(self, imageLeftEye, imageRightEye):
        leftEyeCoordinates = self.cnnLandmarker.predictEyeCorners(imageLeftEye)
        rightEyeCoordinates = self.cnnLandmarker.predictEyeCorners(imageRightEye)        
        return leftEyeCoordinates, rightEyeCoordinates
               
               
    def computeCnnLandmarksForFace(self, imageFace, faceSide):
        return self.cnnLandmarker.predictFaceLandmarks(imageFace, faceSide)
        
        
    # Resets render pipeline: Erase mesh, texture and reset view.
    def reset(self):
        self.renderPipeline.renderer.SetBackground(0.0, 0.0, 0.0)
        self.renderPipeline.renderer.ResetCamera()
        self.texture = None
          
    
    def setRenderView(self,view):
        """ 
        Aux function to set render view.
        Used for testing discretization errors in projection.
        """
        focalPoint = view['focalPoint']
        cameraCoords = view['cameraCoordinates']
        self.renderPipeline.renderer.GetActiveCamera().SetFocalPoint(focalPoint[0], focalPoint[1], focalPoint[2])
        self.renderPipeline.renderer.GetActiveCamera().SetPosition(cameraCoords[0],cameraCoords[1],cameraCoords[2])
   
    
    def projectSingle2DFaceLandmarksOntoMesh(self,landmark2D, view):
        """
        Aux function to project individual landmark onto mesh. For estimation of projection errors.
        """
        picker = self.renderPipeline.renderer.GetRenderWindow().GetInteractor().GetPicker()
        
        self.renderPipeline.setCameraCoords(view['cameraCoordinates'])
        self.renderPipeline.setFocalPoint(view['focalPoint'])
        dims = view['imageDimensions']
        
        lmCoord = landmark2D
        successStatus = picker.Pick(int(lmCoord[0]), 
                            dims-int(lmCoord[1]), 
                            0,  
                            self.renderPipeline.renderer)
        
        if successStatus == 0:
            return (0,0,0)  #TODO throw
        else:
            landmark3D = picker.GetPickPosition()                
            return landmark3D

    
    # Returns left eye image, right eye image, portrait image
    def render2Dsnapshots(self, initial3DLandmarks={}, imageDimensions = 256):
    
        first_axis, second_axis, third_axis = 0, 1, 2
        
        try: 
            i_first_axis_landmark1 = initial3DLandmarks["right_ex"]
            i_first_axis_landmark2 = initial3DLandmarks["left_ex"]
            i_second_axis_landmark1 = initial3DLandmarks["nasion"]
            i_second_axis_landmark2 = initial3DLandmarks["gnathion"]
        except ValueError:
            print("Cannot find axis landmarks, check N landmarks")
            return

        axes = geometryUtils.getUserDefinedAxes(\
                    first_axis, second_axis, third_axis, 
                    i_first_axis_landmark1, i_first_axis_landmark2, 
                    i_second_axis_landmark1, i_second_axis_landmark2)

        focalPoint_LeftEye, cameraCoords_LeftEye = \
            geometryUtils.getCameraCoordsForLeftEye(axes, initial3DLandmarks)
            
        focalPoint_RightEye, cameraCoords_RightEye = \
            geometryUtils.getCameraCoordsForRightEye(axes, initial3DLandmarks)

        focalPoint_LeftSideView, cameraCoords_LeftSideView, roll_LeftSideView = \
            geometryUtils.getCameraCoordsForSideView(
                        axes, initial3DLandmarks, angle = 45)

        focalPoint_RightSideView, cameraCoords_RightSideView, roll_RightSideView = \
            geometryUtils.getCameraCoordsForSideView(
                        axes, initial3DLandmarks, angle = -45)

        focalPoint_Portrait, cameraCoords_Portrait = \
            geometryUtils.getCameraCoordsForPortrait(axes, initial3DLandmarks)

        leftEyeImage = self.renderPipeline.render2DView(focalPoint_LeftEye,
                                                        cameraCoords_LeftEye,
                                                        imageDimensions)
        rightEyeImage = self.renderPipeline.render2DView(focalPoint_RightEye,
                                                         cameraCoords_RightEye,
                                                         imageDimensions)
        leftSideViewImage = self.renderPipeline.render2DView(\
                    focalPoint_LeftSideView,\
                    cameraCoords_LeftSideView,\
                    imageDimensions,\
                    roll=roll_LeftSideView,\
                    label="leftSide")
        rightSideViewImage = self.renderPipeline.render2DView(\
                    focalPoint_RightSideView,\
                    cameraCoords_RightSideView,\
                    imageDimensions,\
                    roll=roll_RightSideView,\
                    label="rightSide")
        portraitImage = self.renderPipeline.render2DView(focalPoint_Portrait,
                                                         cameraCoords_Portrait,
                                                         imageDimensions)
        
        leftEyeView = {'image2D' : leftEyeImage, 
                       'cameraCoordinates' : cameraCoords_LeftEye,
                       'focalPoint' : focalPoint_LeftEye,
                       'imageDimensions' : imageDimensions}
                       
        rightEyeView = {'image2D' : rightEyeImage, 
                        'cameraCoordinates' : cameraCoords_RightEye,
                        'focalPoint' : focalPoint_RightEye,
                        'imageDimensions' : imageDimensions}

        leftSideView = {'image2D' : leftSideViewImage, 
                        'cameraCoordinates' : cameraCoords_LeftSideView,
                        'focalPoint' : focalPoint_LeftSideView,
                        'imageDimensions' : imageDimensions}                        
            
        rightSideView = {'image2D' : rightSideViewImage, 
                         'cameraCoordinates' : cameraCoords_RightSideView,
                         'focalPoint' : focalPoint_RightSideView,
                         'imageDimensions' : imageDimensions}

        portraitView = {'image2D' : portraitImage, 
                        'cameraCoordinates' : cameraCoords_Portrait,
                        'focalPoint' : focalPoint_Portrait,
                        'imageDimensions' : imageDimensions}

        return leftEyeView, rightEyeView, portraitView, leftSideView, rightSideView
        

        #TODO hist eq

    def project2DlandmarksToMesh(self, landmarkList, cameraCoords, focalPt, dims):
        landmarkList3D = {}
        picker = self.renderPipeline.renderWindow.GetInteractor().GetPicker()

        self.renderPipeline.setCameraCoords(cameraCoords)
        self.renderPipeline.setFocalPoint(focalPt)

        for landmark in landmarkList:
            picker.Pick(int(landmarkList[landmark][0]), 
                        dims-int(landmarkList[landmark][1]), 
                        0,  
                        self.renderPipeline.renderer)
            landmarkList3D[landmark] = picker.GetPickPosition()

        return landmarkList3D

       
    def project2DEyeLandmarksOntoMesh(self, leftEyeCoordinates, leftEyeView,\
                                            rightEyeCoordinates, rightEyeView):
       
        picker = self.renderPipeline.renderer.GetRenderWindow().GetInteractor().GetPicker()
        
        self.renderPipeline.setCameraCoords(leftEyeView['cameraCoordinates'])
        self.renderPipeline.setFocalPoint(leftEyeView['focalPoint'])

        dims = leftEyeView['imageDimensions']

        picker.Pick(int(leftEyeCoordinates['en_x']), 
                    dims-int(leftEyeCoordinates['en_y']), 
                    0,  
                    self.renderPipeline.renderer)
        leftEye_Landmark_En = picker.GetPickPosition() #TODO: Case: empty! -> ret 0_z (C++)

        picker.Pick(int(leftEyeCoordinates['ex_x']), 
                    dims-int(leftEyeCoordinates['ex_y']), 
                    0,  
                    self.renderPipeline.renderer)
        leftEye_Landmark_Ex = picker.GetPickPosition()  #TODO: Case: empty!      
                
        self.renderPipeline.setCameraCoords(rightEyeView['cameraCoordinates'])
        self.renderPipeline.setFocalPoint(rightEyeView['focalPoint'])

        dims = rightEyeView['imageDimensions']

        picker.Pick(int(rightEyeCoordinates['en_x']), 
                    dims-int(rightEyeCoordinates['en_y']), 
                    0,  
                    self.renderPipeline.renderer)
        rightEye_Landmark_En = picker.GetPickPosition() #TODO: Case: empty!

        picker.Pick(int(rightEyeCoordinates['ex_x']), 
                    dims-int(rightEyeCoordinates['ex_y']), 
                    0,  
                    self.renderPipeline.renderer)
        rightEye_Landmark_Ex = picker.GetPickPosition() #TODO: Case: empty!

        eyeLandmarks3D = {'left_en'  : leftEye_Landmark_En,  \
                          'left_ex ' : leftEye_Landmark_Ex,  \
                          'right_en' : rightEye_Landmark_En, \
                          'right_ex' : rightEye_Landmark_Ex}
       
        return eyeLandmarks3D
        

    def project2DFaceLandmarksOntoMesh(self, leftFaceCoordinates, leftFaceView,\
                                             rightFaceCoordinates, rightFaceView):
        
        faceLandmarks3D = {}
        missingFaceLandmars3D = []
        
        picker = self.renderPipeline.renderer.GetRenderWindow().GetInteractor().GetPicker()
        
        self.renderPipeline.setCameraCoords(leftFaceView['cameraCoordinates'])
        self.renderPipeline.setFocalPoint(leftFaceView['focalPoint'])

        dims = leftFaceView['imageDimensions']
        
        for landmark in leftFaceCoordinates:
            lmCoord = leftFaceCoordinates[landmark]

            successStatus = picker.Pick(int(lmCoord[0]), 
                                dims-int(lmCoord[1]), 
                                0,  
                                self.renderPipeline.renderer)
            
            # TODO for some landmarks on the periphery, it may be acceptable to increase the 
            # picking tolerance
            if successStatus == 0:
                missingFaceLandmars3D.append(landmark)
            else:
                landmark3D = picker.GetPickPosition()                
                faceLandmarks3D[landmark] = landmark3D
            
        # For now, collect only the remaining landmarks from the right side
        # TODO: better selection scheme, e.g, based on CM and/or geom averaging
        remainingRightFaceCoordinates = \
            ['right_en', 'right_ex', 'right_cupid', 'right_ch', 'right_alare', 
            'lower_right_ear_attachment', 'lower_lip_centre', 'right_tragion'] #'tragion_right'
            
        self.renderPipeline.setCameraCoords(rightFaceView['cameraCoordinates'])
        self.renderPipeline.setFocalPoint(rightFaceView['focalPoint'])

        dims = rightFaceView['imageDimensions']            
            
        for landmark in remainingRightFaceCoordinates:
            lmCoord = rightFaceCoordinates[landmark]

            successStatus = picker.Pick(int(lmCoord[0]), 
                                dims-int(lmCoord[1]), 
                                0,  
                                self.renderPipeline.renderer)
                    
            if successStatus == 0:
                missingFaceLandmars3D.append(landmark)
            else:                                
                landmark3D = picker.GetPickPosition()                 
                faceLandmarks3D[landmark] = landmark3D
                                
        # Scanning the missingFaceLandmars3D, see if those not picked from the left view image
        # can be picked from the right view image
        for missingLM in missingFaceLandmars3D:
            if missingLM in rightFaceCoordinates:
                lmCoord = rightFaceCoordinates[missingLM]
                
                successStatus = picker.Pick(int(lmCoord[0]), 
                    dims-int(lmCoord[1]), 
                    0,  
                    self.renderPipeline.renderer)
                
                if successStatus != 0:
                    missingFaceLandmars3D.remove(missingLM)                              
                    landmark3D = picker.GetPickPosition()                 
                    faceLandmarks3D[missingLM] = landmark3D
               
        return faceLandmarks3D,missingFaceLandmars3D
   
        
        
        
    def project2DErrorFaceLandmarksOntoMesh(self, leftFaceCoordinates, leftFaceView,\
                                                  rightFaceCoordinates, rightFaceView,\
                                                  x_error=1, y_error=1):
        """
        Misc function projecting 2D landmarks with a specified error onto 3D mesh.
        
        Used to estimate impact of 2D source image resolution (e.g., CNN output map) on 3D accuracy.
        """
        
        errorFaceLandmarks3D = {}
        missingErrorFaceLandmarks3D = []
        
        picker = self.renderPipeline.renderer.GetRenderWindow().GetInteractor().GetPicker()
        
        self.renderPipeline.setCameraCoords(leftFaceView['cameraCoordinates'])
        self.renderPipeline.setFocalPoint(leftFaceView['focalPoint'])

        dims = leftFaceView['imageDimensions']
        
        for landmark in leftFaceCoordinates:
            lmCoord = leftFaceCoordinates[landmark]

            x_coord, y_coord = int(lmCoord[0]), dims-int(lmCoord[1])
            
            x_coord, y_coord = int(lmCoord[0]), dims-int(lmCoord[1])
            x_coord, y_coord = x_coord + x_error, y_coord + y_error
            #x_coord = x_coord + x_error if x_coord + x_error < dims else x_coord - x_error
            #y_coord = dims - y_coord + y_error if dims - y_coord + y_error < dims else dims - y_coord - y_error
            
            successStatus = picker.Pick(x_coord, 
                                        y_coord, 
                                        0,  
                                        self.renderPipeline.renderer)
            
            if successStatus == 0:
                missingErrorFaceLandmarks3D.append(landmark)
            else:
                landmark3D = picker.GetPickPosition()                
                errorFaceLandmarks3D[landmark] = landmark3D
            
        remainingRightFaceCoordinates = \
            ['right_en', 'right_ex', 'right_cupid', 'right_ch', 'right_alare', 
            'lower_right_ear_attachment', 'lower_lip_centre', 'right_tragion'] #'tragion_right'
            
        self.renderPipeline.setCameraCoords(rightFaceView['cameraCoordinates'])
        self.renderPipeline.setFocalPoint(rightFaceView['focalPoint'])

        dims = rightFaceView['imageDimensions']            
            
        for landmark in remainingRightFaceCoordinates:
            lmCoord = rightFaceCoordinates[landmark]

            x_coord, y_coord = int(lmCoord[0]), dims-int(lmCoord[1])

            x_coord, y_coord = int(lmCoord[0]), dims-int(lmCoord[1])
            x_coord, y_coord = x_coord + x_error, y_coord + y_error
            #x_coord = x_coord + x_error if x_coord + x_error < dims else x_coord - x_error
            #y_coord = dims - y_coord + y_error if dims - y_coord + y_error < dims else dims - y_coord - y_error
            
            successStatus = picker.Pick(x_coord, 
                                        y_coord, 
                                        0,  
                                        self.renderPipeline.renderer)
                    
            if successStatus == 0:
                missingErrorFaceLandmarks3D.append(landmark)
            else:                                
                landmark3D = picker.GetPickPosition()                 
                errorFaceLandmarks3D[landmark] = landmark3D
                                
        # Scanning the missingFaceLandmars3D, see if those not picked from the left view image
        # can be picked from the right view image
        for missingLM in missingErrorFaceLandmarks3D:
            if missingLM in rightFaceCoordinates:
                lmCoord = rightFaceCoordinates[missingLM]
                
                x_coord, y_coord = int(lmCoord[0]), dims-int(lmCoord[1])
                x_coord, y_coord = x_coord + x_error, y_coord + y_error
                #x_coord = x_coord + x_error if x_coord + x_error < dims else x_coord - x_error
                #y_coord = dims - y_coord + y_error if dims - y_coord + y_error < dims else dims - y_coord - y_error
                
                successStatus = picker.Pick(x_coord, 
                                            y_coord, 
                                            0,  
                                            self.renderPipeline.renderer)
                
                if successStatus != 0:
                    missingErrorFaceLandmarks3D.remove(missingLM)                              
                    landmark3D = picker.GetPickPosition()                 
                    errorFaceLandmarks3D[missingLM] = landmark3D
               
        return errorFaceLandmarks3D,missingErrorFaceLandmarks3D
        
        
    # Returns 3D landmarks; 20 points. 
    #def getLandmarks3D(self):
    #    pass        
