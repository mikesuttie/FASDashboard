import math
import numpy as np

def getLength(p):
    return float(math.sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]))


def isCloseby3D(landmark1, landmark2, tolerance = 2.0):

    d0 = landmark1[0]-landmark2[0]
    d1 = landmark1[1]-landmark2[1]
    d2 = landmark1[2]-landmark2[2]    

    return d0 * d0 + d1 * d1 + d2 * d2 < tolerance * tolerance


def normalize(point):
    length = getLength(point)
    if (length != 0.0):
        point[0] /= length
        point[1] /= length
        point[2] /= length

def distanceBetweenLines(l0, l1, m0, m1, closestPt1, closestPt2, t1, t2):
    # Part of this function was adapted from "GeometryAlgorithms.com"
    # Copyright 2001, softSurfer(www.softsurfer.com)
    # This code may be freely used and modified for any purpose
    u = [l1[0]-l0[0], l1[1]-l0[1], l1[2]-l0[2]]
    v = [m1[0]-m0[0], m1[1]-m0[1], m1[2]-m0[2]]
    w = [l0[0]-m0[0], l0[1]-m0[1], l0[2]-m0[2]]
    a = np.dot(u, u)
    b = np.dot(u, v)
    c = np.dot(v, v) # always >= 0
    d = np.dot(u, w)
    e = np.dot(v, w)

    D: float = a * c - b * b # always >= 0
    # compute the line parameters of the two closest points
    if (D < 1e-6):#lines are almost parallel
        t1 = 0.0
        t2 = 0.0
        if (b > c):
            t2 = d / b
        else:
            t2 = e / c

    else:
        t1 = (b * e - c * d) / D
        t2 = (a * e - b * d) / D

    for i in range(3):
        closestPt1[i] = l0[i] + t1 * u[i]
        closestPt2[i] = m0[i] + t2 * v[i]
    # Return the distance squared between the lines =
    # the mag squared of the distance between the two closest points
    # = L1(t1) - L2(t2)
    return np.linalg.norm(closestPt1-closestPt2)

def getUserDefinedAxes(first_axis, second_axis, third_axis, 
                       i_first_axis_landmark1, i_first_axis_landmark2,
                       i_second_axis_landmark1, i_second_axis_landmark2):

    axes = [[1 for j in range(3)] for i in range(3)]

    #the first axis is given by two landmarks
    p1 = i_first_axis_landmark1
    p2 = i_first_axis_landmark2

    ex1 = [p1[0], p1[1], p1[2]]
    ex2 = [p2[0], p2[1], p2[2]]
    axes[first_axis] = [p1[0] - p2[0], p1[1] - p2[1], p1[2] - p2[2]]
    normalize(axes[first_axis])

    # the second axis is approximately given by two landmarks in a similar fashion
    p1 = [i_second_axis_landmark1[0], i_second_axis_landmark1[1], i_second_axis_landmark1[2]]
    p2 = [i_second_axis_landmark2[0], i_second_axis_landmark2[1], i_second_axis_landmark2[2]]

    v1 = [p1[0], p1[1], p1[2]]
    v2 = [p2[0], p2[1], p2[2]]

    closestPt1 = np.empty(3)
    closestPt2 = np.empty(3)
    t1 = 0
    t2 = 0
    # Alter the 2nd set of axes.M.Suttie
    distanceBetweenLines(ex1, ex2, v1, v2, closestPt1, closestPt2, t1, t2)
    # Alter
    p1[0] += closestPt2[0] - closestPt1[0]
    p1[1] += closestPt2[1] - closestPt1[1]
    p1[2] += closestPt2[2] - closestPt1[2]

    p2[0] += closestPt2[0] - closestPt1[0]
    p2[1] += closestPt2[1] - closestPt1[1]
    p2[2] += closestPt2[2] - closestPt1[2]

    axes[second_axis] = [p1[0] - p2[0], p1[1] - p2[1], p1[2] - p2[2]]
    dotp = np.dot(axes[first_axis], axes[second_axis])
    axes[second_axis][0] -= axes[first_axis][0]*dotp
    axes[second_axis][1] -= axes[first_axis][1]*dotp
    axes[second_axis][2] -= axes[first_axis][2]*dotp

    normalize(axes[second_axis])
    # check
    error = np.dot(np.array(axes[first_axis]), np.array(axes[second_axis]))
    assert error < 1e-5
    # then     the    third    axis is simply    orthogonal    to    the    first    two
    third_axis = 3 - first_axis - second_axis
    axes[third_axis] = np.cross(np.array(axes[first_axis]), np.array(axes[second_axis]))
    error = np.dot(np.array(axes[first_axis]), np.array(axes[third_axis]))
    assert error < 1e-5
    error = np.dot(np.array(axes[second_axis]), np.array(axes[third_axis]))
    assert error < 1e-5

    return axes
    

    
    
def getCameraCoordsForLeftEye(axes, landmarksList):
    ex = np.empty(3)
    en = np.empty(3)
    focal_point = np.empty(3)
    cameraCoords = np.empty(3)
 
    try:
        ex = landmarksList["left_ex"]
        en = landmarksList["left_en"]

    except ValueError:
        print("Cannot find landmarks, check landmarks")
        return

    #focal point for eyes is the midpoint between ex and en
    focal_point[0] = float(ex[0] + en[0]) / 2.0
    focal_point[1] = float(ex[1] + en[1]) / 2.0
    focal_point[2] = float(ex[2] + en[2]) / 2.0
    cameraCoords[0] = focal_point[0] - (axes[2][0] * 70)
    cameraCoords[1] = focal_point[1] - (axes[2][1] * 70)
    cameraCoords[2] = focal_point[2] - (axes[2][2] * 70)    
    
    return  focal_point, cameraCoords
    
    
def getCameraCoordsForRightEye(axes, landmarksList):
    ex = np.empty(3)
    en = np.empty(3)
    focal_point = np.empty(3)
    cameraCoords = np.empty(3)
 
    try:
        ex = landmarksList["right_ex"]
        en = landmarksList["right_en"]

    except ValueError:
        print("Cannot find landmarks, check landmarks")
        return

    #focal point for eyes is the midpoint between ex and en
    focal_point[0] = float(ex[0] + en[0]) / 2.0
    focal_point[1] = float(ex[1] + en[1]) / 2.0
    focal_point[2] = float(ex[2] + en[2]) / 2.0
    cameraCoords[0] = focal_point[0] - (axes[2][0] * 70)
    cameraCoords[1] = focal_point[1] - (axes[2][1] * 70)
    cameraCoords[2] = focal_point[2] - (axes[2][2] * 70)    
    
    return  focal_point, cameraCoords    
    
    
    
def getCameraCoordsForPortrait(axes,  landmarksList):
    focal_point = np.empty(3) # TODO Check this is the intended default!
    cameraCoords = np.empty(3)
    PROJ_VALUE = -250  # projection value to move camera along z axis
    try:
        subnasale = landmarksList["subnasale"]
    except ValueError:
        print("Cannot find landmarks, check landmarks")
        return
    cameraCoords[0] = subnasale[0] + (axes[2][0] * PROJ_VALUE)
    cameraCoords[1] = subnasale[1] + (axes[2][1] * PROJ_VALUE)
    cameraCoords[2] = subnasale[2] + (axes[2][2] * PROJ_VALUE)    
    
    return  focal_point, cameraCoords    
    

def getCameraCoordsForSideView(axes, landmarksList, angle = 45):

    focal_point = np.empty(3)
    cameraCoords = np.empty(3)
    roll = 0
    
    # use portrait projected point and rotate 45 along Z axis
    x = 0
    y = 1
    z = 2
    try:
        left_ex = landmarksList["left_ex"]
        right_ex = landmarksList["right_ex"]
        nasion =  landmarksList["nasion"]
        gnathion = landmarksList["gnathion"]
        subnasale = landmarksList["subnasale"]

    except ValueError:
        print("Cannot find landmarks, check landmarks")
        return

    #get facial height as a metric to scale
    facialHeight = math.sqrt((nasion[x] - gnathion[x])**2 \
                           + (nasion[y] - gnathion[y])**2 \
                           + (nasion[z] - gnathion[z])**2)

    #set the initial camera position along the z axis projeted by 2fn
    # roll = angle between wordl X axis and X userdefined#

    worldX = np.array([0,1,0])
    magWorld = np.sqrt(np.dot(worldX, worldX))
    magDefined = np.sqrt(np.dot(axes[1], axes[1]))
    dot = np.dot(worldX, axes[1])
    rotAngle = np.arccos(np.dot(worldX, axes[1]) \
            / (magWorld * magDefined)) * (180/math.pi) 

    if axes[2][1] >= 0:
        rotAngle = -rotAngle

    if angle > 0:
        focal_point[0] = left_ex[0]
        focal_point[1] = left_ex[1]
        focal_point[2] = left_ex[2]
        roll = rotAngle

    else:
        focal_point[0] = right_ex[0]
        focal_point[1] = right_ex[1]
        focal_point[2] = right_ex[2]
        roll = -rotAngle

    #move the focus point down a bit
    focal_point[x] -= axes[y][x] * (facialHeight*0.4)
    focal_point[y] -= axes[y][y] * (facialHeight*0.4)
    focal_point[z] -= axes[y][z] * (facialHeight*0.4)

    cameraCoords[0] = subnasale[x] + axes[z][x] * (-facialHeight * 2) \
                                   - axes[y][x] * (facialHeight * 0.6)
    cameraCoords[1] = subnasale[y] + axes[z][y] * (-facialHeight * 2) \
                                   - axes[y][y] * (facialHeight * 0.6)
    cameraCoords[2] = subnasale[z] + axes[z][z] * (-facialHeight * 2) \
                                   - axes[y][z] * (facialHeight * 0.6)

    if angle > 0:
        cameraCoords[0] -= axes[x][x] * (facialHeight * 2)
        cameraCoords[1] -= axes[x][y] * (facialHeight * 2)
        cameraCoords[2] -= axes[x][z] * (facialHeight * 2)
    else:
        cameraCoords[0] += axes[x][x] * (facialHeight * 2)
        cameraCoords[1] += axes[x][y] * (facialHeight * 2)
        cameraCoords[2] += axes[x][z] * (facialHeight * 2)

    return  focal_point, cameraCoords, roll
    
    

