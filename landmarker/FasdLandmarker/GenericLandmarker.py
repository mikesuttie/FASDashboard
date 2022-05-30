from fastapi import HTTPException, status

import dlib
import cv2
import os.path
import numpy as np
from imutils import face_utils

def get_Dlib_68pt_faceLandmarkModel():
    """ Load a face landmark predictor.
    Note: 68pt model is for non-commercial use.
    Potential alternatives, see:
    https://stackoverflow.com/questions/36908402/dlib-training-shape-predictor-for-194-landmarks-helen-dataset
    http://sourceforge.net/projects/dclib/files/dlib/v18.10/shape_predictor_68_face_landmarks.dat.bz2
    """   
    dlib_shape_predictor_filename = os.path.join("data","models","dlib", "shape_predictor_68_face_landmarks.dat") 
    if not os.path.exists(dlib_shape_predictor_filename):        
        return None
        
    predictor = dlib.shape_predictor(os.path.abspath(dlib_shape_predictor_filename))

    return predictor
    
    
def predictGenericLandmarks(predictor, image):

    landmarks = np.empty((68, 2))
    #image = cv2.imread(exampleImage)
    # TODO replace: grayScaleImage = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    grayScaleImage = image #.dot([0.07, 0.72, 0.21])
    #grayScaleImage = np.min(grayScaleImage, 255).astype(np.uint8)

    detector = dlib.get_frontal_face_detector()
    rects = detector(grayScaleImage, 1)

    # Case no face or multiple faces detected, or degenerate detection!
    if(len(rects)!=1):
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Dlib face detector did not detect exactly one face. Rectangles: " + str(rects),
            headers={"WWW-Authenticate": "Bearer"},
        )
    #print('DLIB RECTS PREDICTED:', rects)
    
    shape = predictor(grayScaleImage, rects[0])    
    shape = face_utils.shape_to_np(shape)

    #print('DLIB SHAPE PREDICTED:', shape)
    
    for i, (x, y) in enumerate(shape):
        landmarks[i] = [int(x), int(y)]
        #print('Landmark',i,(x,y))
        #cv2.circle(image, (x, y), 1, (0, 255, 0), -1)
    
    selectedLandmarks = {
        "left_ex": [shape.item((45, 0)), shape.item((45, 1))],
        "left_en": [shape.item((42, 0)), shape.item((42, 1))],
        "right_ex": [shape.item((36, 0)), shape.item((36, 1))],
        "right_en": [shape.item((39, 0)), shape.item((39, 1))],
        # calcualte nasion - midpoint of lm 21, 22, 27
        "nasion": [(shape.item((21, 0))+shape.item((22, 0))+shape.item((27, 0))) / 3,
              (shape.item((21, 1)) + shape.item((22, 1)) + shape.item((27, 1))) / 3],
        "gnathion": [shape.item((8, 0)), shape.item((8, 1))],
        "subnasale": [shape.item((33, 0)), shape.item((33, 1))],
        
        #Additional landmarks that may be useful if subsequent CNN methods fail:
        "left_alare": [shape.item((35, 0)), shape.item((35, 1))],            
        "right_alare": [shape.item((31, 0)), shape.item((31, 1))],
        "left_ch": [shape.item((54, 0)), shape.item((54, 1))],
        "right_ch": [shape.item((48, 0)), shape.item((48, 1))],        
        "left_cupid": [shape.item((52, 0)), shape.item((52, 1))],        
        "right_cupid": [shape.item((50, 0)), shape.item((50, 1))], 
        "lower_lip_centre": [shape.item((57, 0)), shape.item((57, 1))], 
        "lip_centre": [shape.item((51, 0)), shape.item((51, 1))
        
        # Landmarks still missing (to be contributed by subsequent detectors): 
        #    ear_attachment 2x, tragion 2x, pronasale
        ]
    }

    return selectedLandmarks
   
   
def drawLandmarksIntoImage(image, landmarks):
    print ('2D landmarks:',landmarks)
    #print (image.shape)
    if landmarks is None:
        print('******* NO LANDMARKS DETECTED *****!')
        return image
    
    for lm in landmarks:
        landmark = landmarks[lm]
        x, y = landmark
        #print('XY',x,y)
        #cv2.circle(image, (x, y), 1, (0, 0, 255), -1)

        image[int(y), int(x)] = (0, 255, 244)         

    return image
   
