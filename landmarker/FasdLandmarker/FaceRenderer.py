import numpy as np
import vtk
#from vtk.util import numpy_support
import utils
import cv2


class FaceRenderPipeline:
    def __init__(self, imageDimension=512):

        self.objReader = vtk.vtkOBJReader()         
        #self.objReader.SetFileName('data/subjects/JWM5671.obj')         # TODO TODO TODO fix this! Use some dummy data?
                        
        self.texture = vtk.vtkTexture()        

        self.mapper = vtk.vtkPolyDataMapper()
        self.mapper.SetInputConnection(self.objReader.GetOutputPort())

        self.actor = vtk.vtkActor()
        self.actor.SetMapper(self.mapper)
        self.actor.SetTexture(self.texture)
        self.actor.GetProperty().LightingOff()

        self.renderer = vtk.vtkRenderer()
        self.renderer.SetBackground(0.0, 0.0, 0.0)
        self.renderer.ResetCamera()
        self.renderer.AddActor(self.actor)

        renderWindow = vtk.vtkRenderWindow()
        renderWindow.SetOffScreenRendering(True)
        renderWindow.AddRenderer(self.renderer)        

        #self.imageData = vtk.vtkImageData()

        self.windowToImageFilter = vtk.vtkWindowToImageFilter()
        self.windowToImageFilter.SetInput(renderWindow) # TODO: Does this construct a pipeline, or 'SetInputConnection' required?
        self.windowToImageFilter.SetInputBufferTypeToRGB()
        self.windowToImageFilter.ReadFrontBufferOff()  # MS: read from the back buffer

        # Interactor required to pick 3D points on mesh, using intial 2D landmarks from 2D detector (e.g., dlib 68pt model)
        self.iren = vtk.vtkRenderWindowInteractor()
        self.iren.SetRenderWindow(renderWindow)

          
    def setCameraCoords(self, cameraCoords):
        self.renderer.GetActiveCamera().SetPosition(cameraCoords[0],cameraCoords[1],cameraCoords[2])
        
    def setFocalPoint(self, focalPoint):
        self.renderer.GetActiveCamera().SetFocalPoint(focalPoint[0], focalPoint[1], focalPoint[2])
        
    def setRoll(self, roll):        
        self.renderer.GetActiveCamera().SetRoll(roll)
                
    def loadMeshFromObjFile(self, meshFilePath):        
        self.objReader.SetFileName(meshFilePath) #TODO check success (catch) at time of rendering
        print('UPDATED OBJ READER: PATH:',meshFilePath)
        
    def loadTextureFromPngFile(self, textureFilePath):
        self.textureReader = vtk.vtkPNGReader()
        #textureFilePath='/home/rr/fasd/repos/FasdLandmarker/FasdLandmarker/data/subjects/JWM5671.png'
        self.textureReader.SetFileName(textureFilePath)

        
    def renderFrontalView(self,imageDimensions=512):

        focalPoint = np.zeros(3)
        cameraCoords = np.zeros(3)

        camera = vtk.vtkCamera()
        camera.SetPosition( 0.0, 0.0, 500.0)#cameraCoords[2])        
        camera.SetFocalPoint(focalPoint[0],focalPoint[1],focalPoint[2])
        
        self.renderer.SetActiveCamera(camera)
        #self.renderer.ResetCamera()
        
        cam = self.renderer.GetActiveCamera()
        #px,py,pz,fx,fy,fz = 999,999,999,999,999,999
        #px,py,pz = cam.GetPosition()
        #fx,fy,fz = cam.GetFocalPoint()
        print("(((((((((((((((((((((((((((cam.GetPosition() cam.GetFocalPoint()", cam.GetPosition(), cam.GetFocalPoint())

        self.renderer.GetRenderWindow().SetSize(imageDimensions,imageDimensions)
        self.renderer.GetRenderWindow().Render()
        self.windowToImageFilter.Update()
        self.windowToImageFilter.Modified() #Filter won't update without this call.
        
        vtkImage = self.windowToImageFilter.GetOutput()

        # https://stackoverflow.com/questions/14553523/vtk-render-window-image-to-numpy-array        
        width, height, _ = vtkImage.GetDimensions()
        vtkArray = vtkImage.GetPointData().GetScalars()
        components = vtkArray.GetNumberOfComponents()

        img = utils.vtkToNumpy(vtkImage)

        # TODO: Layout of the output array dst is incompatible 
        # .......... with cv::Mat (step[ndims-1] != elemsize or step[1] != elemsize*nchannels)
        # ycrcb = cv2.cvtColor(img, cv2.COLOR_BGR2YCR_CB)
        # channels = cv2.split(ycrcb)
        # cv2.equalizeHist(channels[0], channels[0])
        # cv2.merge(channels, ycrcb)
        # cv2.cvtColor(ycrcb, cv2.COLOR_YCR_CB2BGR, img)
        
       
        return img
        

                
    
    # TODO note: almost duplicate of fct renderFrontalView()    
    def render2DView(self, focalPoint, cameraCoords,imageDimensions, roll=0, label=""):

        #print('RENDER2DVIEW:',focalPoint,cameraCoords)
        #self.renderer.ResetCamera()
        #self.iren.Disable()
        #self.setCameraCoords(cameraCoords)
        #self.setFocalPoint(focalPoint)

        camera = vtk.vtkCamera()
        camera.SetPosition( cameraCoords[0], cameraCoords[1], cameraCoords[2])        
        camera.SetFocalPoint(focalPoint[0],focalPoint[1],focalPoint[2])
        camera.SetRoll(roll)
        self.renderer.SetActiveCamera(camera)

        #self.renderer.GetActiveCamera().SetPosition( cameraCoords[0], cameraCoords[1], cameraCoords[2])        
        #self.renderer.GetActiveCamera().SetFocalPoint(focalPoint[0],focalPoint[1],focalPoint[2])
        
        #self.iren.Enable()
        #self.iren.EnableRenderOn()
        
        renderWindow = self.renderer.GetRenderWindow()
        renderWindow.SetSize(imageDimensions,imageDimensions)
        self.windowToImageFilter = vtk.vtkWindowToImageFilter() # Cannot be reused?
        self.windowToImageFilter.SetInputBufferTypeToRGB()
        self.windowToImageFilter.ReadFrontBufferOff()  # MS: read from the back buffer        
        self.windowToImageFilter.SetInput(renderWindow) 
        renderWindow.Render() 
        self.windowToImageFilter.Update()
        self.windowToImageFilter.Modified() #'Update the modification time for this object. '
        
        vtkImage = self.windowToImageFilter.GetOutput()
        width, height, _ = vtkImage.GetDimensions()
        vtkArray = vtkImage.GetPointData().GetScalars()
        components = vtkArray.GetNumberOfComponents()

        img = utils.vtkToNumpy(vtkImage)

        ycrcb = cv2.cvtColor(img, cv2.COLOR_BGR2YCR_CB)
        channels = cv2.split(ycrcb)
        cv2.equalizeHist(channels[0], channels[0])
        cv2.merge(channels, ycrcb)
        cv2.cvtColor(ycrcb, cv2.COLOR_YCR_CB2BGR, np.array(img))

        if label != "":
            cv2.imwrite("faceSideViewTest_"+label+".png", img)

        return img
        #numpy_support.vtk_to_numpy(vtkArray).reshape(height, width, components)
    
