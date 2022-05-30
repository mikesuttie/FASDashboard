import vtk
import numpy as np
from vtk.util import numpy_support

"""
Utility functions for geometry data type conversion
"""


# MS: annoyingly vtk messes up obj loading if there is any marerial ifo from H1 cameras
def RemoveOBJMaterial(file):
    try:
        with open(file, "r") as f:

            lines = f.readlines()

        with open(file, "w") as f:
            for line in lines:
                if "usemtl" not in line:
                    f.write(line)
    except:
        print("Couldn't find file \n")


"""
def EqualizeIntensity(imageFilename):
    img = cv2.imread(imageFilename, 1)
    ycrcb = cv2.cvtColor(img, cv2.COLOR_BGR2YCR_CB)
    channels = cv2.split(ycrcb)
    cv2.equalizeHist(channels[0], channels[0])
    cv2.merge(channels, ycrcb)
    cv2.cvtColor(ycrcb, cv2.COLOR_YCR_CB2BGR, img)
    return img
"""


# from https://stackoverflow.com/questions/25230541/how-to-convert-a-vtkimage-into-a-numpy-array
# .. correction for opencv BGR byte order
def vtkImgToNumpyArray(vtkImageData):
    rows, cols, _ = vtkImageData.GetDimensions()
    scalars = vtkImageData.GetPointData().GetScalars()
    resultingNumpyArray = numpy_support.vtk_to_numpy(scalars)
    resultingNumpyArray = resultingNumpyArray.reshape(cols, rows, -1)
    red, green, blue, alpha = np.dsplit(resultingNumpyArray, resultingNumpyArray.shape[-1])
    resultingNumpyArray = np.stack([blue, green, red, alpha], 2).squeeze()
    resultingNumpyArray = np.flip(resultingNumpyArray, 0)
    return resultingNumpyArray
    

# from https://discourse.vtk.org/t/convert-vtk-array-to-numpy-array/3152
def vtkToNumpy(data):
    temp = numpy_support.vtk_to_numpy(data.GetPointData().GetScalars())
    dims = data.GetDimensions()
    component = data.GetNumberOfScalarComponents()
    if component == 1:
        numpy_data = temp.reshape(dims[2], dims[1], dims[0])
        #numpy_data = numpy_data.transpose(2,1,0) #TODO check if this transpose should happen
    elif component == 3 or component == 4:
        if dims[2] == 1: # a 2D RGB image
            numpy_data = temp.reshape(dims[1], dims[0], component)
            numpy_data = numpy_data.transpose(0, 1, 2)
            numpy_data = np.flipud(numpy_data)
        else:
            raise RuntimeError('unknow type')
    return numpy_data

def numpyToVTK(data, multi_component=False, type='float'):
    '''
    multi_components: rgb has 3 components
    type：float or char
    '''
    if type == 'float':
        data_type = vtk.VTK_FLOAT
    elif type == 'char':
        data_type = vtk.VTK_UNSIGNED_CHAR
    else:
        raise RuntimeError('unknown type')
    if multi_component == False:
        if len(data.shape) == 2:
            data = data[:, :, np.newaxis]
        flat_data_array = data.transpose(2,1,0).flatten()
        vtk_data = numpy_to_vtk(num_array=flat_data_array, deep=True, array_type=data_type)
        shape = data.shape
    else:
        assert len(data.shape) == 3, 'only test for 2D RGB'
        flat_data_array = data.transpose(1, 0, 2)
        flat_data_array = np.reshape(flat_data_array, newshape=[-1, data.shape[2]])
        vtk_data = numpy_to_vtk(num_array=flat_data_array, deep=True, array_type=data_type)
        shape = [data.shape[0], data.shape[1], 1]
    img = vtk.vtkImageData()
    img.GetPointData().SetScalars(vtk_data)
    img.SetDimensions(shape[0], shape[1], shape[2])
    return img


# From https://stackoverflow.com/questions/49783981/using-3-channel-image-in-numpy-as-texture-image-in-vtk
def numpyToVtkTexture(image = np.random.randn(2048, 1024, 3)):    # Assuming a 3D NumPy array as `image`

    grid = vtk.vtkImageData()
    grid.SetDimensions(image.shape[1], image.shape[0], 1)
    vtkarr = numpy_support.numpy_to_vtk(np.flip(image.swapaxes(0,1), axis=1).reshape((-1, 3), order='F'))
    vtkarr.SetName('Image')
    grid.GetPointData().AddArray(vtkarr)
    grid.GetPointData().SetActiveScalars('Image')

    vtex = vtk.vtkTexture()
    vtex.SetInputDataObject(grid)
    vtex.Update()


def numpyToVtkImageData(texture):# = np.random.randn(2048, 1024, 3)): #TODO remove default arg

    grid = vtk.vtkImageData()
    grid.SetDimensions(texture.shape[1], texture.shape[0], 1)
    vtkarr = numpy_support.numpy_to_vtk(np.flip(texture.swapaxes(0,1), axis=1).reshape((-1, 3), order='F'))
    vtkarr.SetName('Texture')
    grid.GetPointData().AddArray(vtkarr)
    grid.GetPointData().SetActiveScalars('Texture')
    return grid

