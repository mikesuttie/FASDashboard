import imageio

import sys
import os.path
import numpy as np

import cv2 #for intensity equalization (required in predicting eye model coordinates)

import time

from cnnPrediction.build_model2D import tf_hm,build_HG
import cnnPrediction.config_dsnt2D as cg
from keras_vggface.utils import preprocess_input

import os
import tensorflow as tf
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'

# Extract and upscale image coordinates of detected landmarks
def extractEyeLandmarks(cnnOutImage):
    for p in cnnOutImage: # Single iteration
        prediction = {}
            
        coords_ex = p[:, :, 0]
        y, x = np.unravel_index(coords_ex.argmax(), coords_ex.shape) # TODO: Is simple argmax appropriate?
        prediction['ex_x'] = x*4 
        prediction['ex_y'] = y*4 
        #cv2.imwrite("eye_ex.png", coords_ex * 64 + 127)
        coords_en = p[:, :, 1]
        y, x = np.unravel_index(coords_en.argmax(), coords_en.shape)
        prediction['en_x'] = x*4 
        prediction['en_y'] = y*4 
        #cv2.imwrite("eye_en.png", coords_en * 64 + 127)      
        
        print ("Detected EYE landmarks 2D: ", prediction)
                     
        return prediction


def extractPortraitLandmarks(cnnOutImage):

    landmark_names = ['none', 'right_ex_x', 'right_ex_y','right_en_x','right_en_y', 
                      'left_en_x', 'left_en_y', 'left_ex_x', 'left_ex_y', 
                      'left_cupid_x', 'left_cupid_y', 'lip_centre_x', 'lip_centre_y', 
                      'right_cupid_x', 'right_cupid_y', 'right_ch_x', 'right_ch_y',
                      'left_ch_x', 'left_ch_y', 'nasion_x', 'nasion_y', 'gnathion_x', 'gnathion_y', 
                      'left_alare_x','left_alare_y', 'right_alare_x', 'right_alare_y',
                      'pronasale_x','pronasale_y', 'subnasale_x', 'subnasale_y', 
                      'lower_right_ear_attachment_x', 'lower_right_ear_attachment_y', 
                      'lower_left_ear_attachment_x', 'lower_left_ear_attachment_y',
                      'lower_lip_centre_x', 'lower_lip_centre_y', 
                      'right_tragion_x','right_tragion_y', 'left_tragion_x','left_tragion_y'] #zeyu was: 'tragion_left_x', 'tragion_left_y']
                      
    for p in cnnOutImage: # Only one iteration per image/file
    
        prediction = {}
        #for m in range(20):
        for m in range(20): 
            hmi = p[:, :, m]
           
            y, x = np.unravel_index(hmi.argmax(), hmi.shape)
            index_x = 2*m+1
            index_y = 2*m+2            
            landmarkName = landmark_names[index_x][:-2]
            prediction[landmarkName] = (4*x, 4*y) # Factor 4 to scale to corresponding image of size 256*256
        return prediction                      


def extractFaceLandmarks(cnnOutImage, faceSide):    

    landmark_names_left = ['none', 'left_en_x', 'left_en_y', 'left_ex_x',
                          'left_ex_y','left_cupid_x', 'left_cupid_y', 'lip_centre_x',
                          'lip_centre_y', 'left_ch_x','left_ch_y', 'nasion_x',
                          'nasion_y','gnathion_x', 'gnathion_y', 'left_alare_x', 
                          'left_alare_y', 'pronasale_x', 'pronasale_y','subnasale_x',
                          'subnasale_y','lower_left_ear_attachment_x',
                          'lower_left_ear_attachment_y','lower_lip_centre_x',
                          'lower_lip_centre_y','left_tragion_x', 'left_tragion_y'] #zeyu was: 'tragion_left_x', 'tragion_left_y']

    landmark_names_right = ['none', 'right_en_x', 'right_en_y', 'right_ex_x',
                           'right_ex_y','right_cupid_x', 'right_cupid_y', 'lip_centre_x',
                           'lip_centre_y', 'right_ch_x','right_ch_y', 'nasion_x',
                           'nasion_y','gnathion_x', 'gnathion_y', 'right_alare_x', 
                           'right_alare_y', 'pronasale_x', 'pronasale_y','subnasale_x',
                           'subnasale_y','lower_right_ear_attachment_x',
                           'lower_right_ear_attachment_y','lower_lip_centre_x',
                           'lower_lip_centre_y','right_tragion_x', 'right_tragion_y']

    landmark_names = landmark_names_left if faceSide == "left"  else (landmark_names_right 
                                         if faceSide == "right" else []) 

    for p in cnnOutImage: # Only one iteration per image/file
    
        prediction = {}
        for m in range(13):           
            hmi = p[:, :, m]
           
            #cv2.imwrite("faceLM"+str(m)+".png", hmi * 100 + 127)
            
            y, x = np.unravel_index(hmi.argmax(), hmi.shape)
            index_x = 2*m+1
            index_y = 2*m+2
            # Factor 4 to scale to corresponding imapreprocess_inputge of size 256*256
            landmarkName = landmark_names[index_x][:-2]
            prediction[landmarkName] = (4*x, 4*y)
        return prediction


class CNN2dLandmarker:

    def __init__(self, face_portrait_detector_model_path
                           = os.path.join("data","models","zeyuCNN", "tf_hm_e80_b8_portrait.hdf5"),
                       eye_detector_model_path 
                           = os.path.join("data","models","zeyuCNN", "build_HG_e50_b8.hdf5"), #"build_HG_e50_b8.hdf5"
                       face_45deg_detector_model_path
                           = os.path.join("data","models","zeyuCNN", "tf_hm_e50_b8.hdf5")): 

        bat_sz=cg.test_batch_sz       

        self.graphFacePortrait = tf.Graph()
        with self.graphFacePortrait.as_default():
            self.sessionFacePortrait = tf.Session()
            with self.sessionFacePortrait.as_default():
                # K.get_session() is sessionFacePortrait
        
                # TODO: return error message. Incomplete initialisation should be prevented!
                self.face_portrait_model = tf_hm(input_dim=[256,256,3], output_dim = 20)                          
                try:
                    # WARNING: Requires 'h5py<3.0.0', else str.decode error!
                    self.face_portrait_model.load_weights(face_portrait_detector_model_path) 
                except ValueError:
                    print('Portrait detection model does not exist!')
                except:
                    print('Portrait detection model loading error. Exiting.')


        self.graphFace45deg = tf.Graph()
        with self.graphFace45deg.as_default():
            self.sessionFace45deg = tf.Session()
            with self.sessionFace45deg.as_default():
                # K.get_session() is sessionFace45deg

                self.face_model_45deg = tf_hm(input_dim=[256,256,3], output_dim = 13)
                try:
                    # WARNING: Requires 'h5py<3.0.0', else str.decode error!
                    self.face_model_45deg.load_weights(face_45deg_detector_model_path) 
                except ValueError:
                    print('Face 45 deg  detection model does not exist!')
                except:
                    print('Face 45 deg detection model loading error. Exiting.')

        self.graphEyeModel = tf.Graph()
        with self.graphEyeModel.as_default():
            self.sessionEyeModel = tf.Session()
            with self.sessionEyeModel.as_default():
                # K.get_session() is sessionEyeModel


                self.eye_model = build_HG()                 
                try:
                    # WARNING: Requires 'h5py<3.0.0', else str.decode error!
                    self.eye_model.load_weights(eye_detector_model_path)                     
                except ValueError:
                    print('Eye detection model does not exist!')
                except:
                    print('Eye detection model loading error. Exiting.')
        
        
    def histeq(im,nbr_bins=256):

       #get image histogram
       imhist,bins = histogram(im.flatten(),nbr_bins,normed=True)
       cdf = imhist.cumsum() #cumulative distribution function
       cdf = 255 * cdf / cdf[-1] #normalize

       #use linear interpolation of cdf to find new pixel values
       im2 = interp(im.flatten(),bins[:-1],cdf)

       return im2.reshape(im.shape), cdf    
        
        
    def predictEyeCorners(self,image):
        with self.graphEyeModel.as_default():
            with self.sessionEyeModel.as_default():
                image_heigt, image_width = 256,256 #TODO get from param struct
                no_channels = 3        
                test_images = np.ndarray((1, image_heigt, image_width, no_channels), dtype=np.float32)
                    
                doHistEq = False
                if doHistEq:
                    #print("EYEEEEEEEEEEEEEEE image shjape" , image[:,:,0].shape)
                    imageio.imwrite('dbg_EyebeforeNormalize.png', image)     
                    im_bgr = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)
                    
                    
                    #EqualizeIntensity
                    ycrcb = cv2.cvtColor(im_bgr, cv2.COLOR_BGR2YCR_CB)
                    channels = cv2.split(ycrcb)
                    cv2.equalizeHist(channels[0], channels[0])
                    cv2.merge(channels, ycrcb)
                    cv2.cvtColor(ycrcb, cv2.COLOR_YCR_CB2BGR, im_bgr)
                    
                    image = cv2.cvtColor(im_bgr, cv2.COLOR_BGR2RGB)
                    imageio.imwrite('dbg_EyeafterNormalize.png', image)                     
                    
                test_images[0, :, :, :] = image/255.0  # normalize

                batch_size = 1
                start = time.time()
                pred = self.eye_model.predict(test_images, batch_size=batch_size)
                
                for i,p in enumerate(pred):
                    imageio.imwrite('dbg_Eyepredex'+str(i)+'.png', p[:, :, 0])
                    imageio.imwrite('dbg_Eyepreden'+str(i)+'.png', p[:, :, 1])                                                          
                print("--- Eye detection time: ", time.time()-start)
                
                return extractEyeLandmarks(pred)
        
    
    # Using models for 45 degree detection    
    def predictFaceLandmarks(self,image, faceSide):
        # Downloading data from 
        # https://github.com/rcmalli/keras-vggface/releases/download/v2.0/rcmalli_vggface_tf_notop_resnet50.h5
        with self.graphFace45deg.as_default():
            with self.sessionFace45deg.as_default():
                image_heigt, image_width = 256,256
                no_channels = 3        
                test_images = np.ndarray((1, image_heigt, image_width, no_channels), dtype=np.float32)
                        
                test_images[0, :, :, :] = preprocess_input(image/ 1.0, version=2)  # 'standardise'

                batch_size = 8
                start = time.time()
                pred = self.face_model_45deg.predict(test_images, batch_size=batch_size)
                print("--- Face detection time: ", time.time()-start)        
                return extractFaceLandmarks(pred, faceSide)


    # Using models for detection in fronto-parallel face pose
    def predictPortraitLandmarks(self,image):
        with self.graphFacePortrait.as_default():
            with self.sessionFacePortrait.as_default():
                image_heigt, image_width = 256,256
                no_channels = 3        
                test_images = np.ndarray((1, image_heigt, image_width, no_channels), dtype=np.float32)
                
                imageio.imwrite('dbg_beforeNormalize.png', image)        
                print("BNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNormalize: ", image.min(), image.max(), image[2][:,0].shape)                
                test_images[0, :, :, :] = preprocess_input(image/ 1.0, version=2)  # 'standardise'
                imageio.imwrite('dbg_afterNormalize.png', test_images[0, :, :, :]) 
                print("ANNNNNNNNNNNNNNNNNNNNNNNNNNNNNNormalize: ", test_images[0, :, :, :].min(), test_images[0, :, :, :].max(), test_images[0, :, :, :][:,0].shape)

                batch_size = 8
                start = time.time()
                pred = self.face_portrait_model.predict(test_images, batch_size=batch_size)
                print("--- Portrait landmarks detection time: ", time.time()-start, "portrait model result shape: ", pred.shape)        
                return extractPortraitLandmarks(pred)

