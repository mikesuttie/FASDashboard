# -*- coding: utf-8 -*-
"""

@author: Zeyu Fu
"""
import csv
import sys
import numpy as np
import os
import config_dsnt2D as cg
import scipy.io as sio
#import matplotlib.pyplot as plt
#import cv2

# Get the size of images
params = cg.params
# Get the list of names of training  images
partition = cg.partition  # IDs
# input_d = [params['dim_img'], params['dim_img'], 3]
cwd = os.getcwd()



def scaleNPYprediction(predictionArray):
    columns = ['File', 'ex_x', 'ex_y', 'en_x','en_y']
    for p in predictionArray: # Should be only 1 iteration
        prediction = {}
        for m in range(2):
            hmi = p[:, :, m]

            y, x = np.unravel_index(hmi.argmax(), hmi.shape)
            index_x = 2*m+1
            index_y = 2*m+2
            prediction[columns[index_x]] = x*4 # this is to recover the resolution to 256*256
            prediction[columns[index_y]] = y*4 # this is to recover the resolution to 256*256
        return prediction




def get_predictions(s,columns):
    predictions = []
    # read the data
    pred_dir = os.path.join(cwd, 'test_predictions') 
    pred =np.load(os.path.join(pred_dir, 'JWMpred.npy'))
    result_landmark_dir =os.path.join(cwd, 'outputs') 
    count = 0 
    img_list = partition['test']
    
    for p in pred:
        
        prediction = {}
        baseName = os.path.splitext(img_list[count])[0]
        prediction['File'] = baseName

        for m in range(2):
            hmi = p[:, :, m]

            y, x = np.unravel_index(hmi.argmax(), hmi.shape)
            index_x = 2*m+1
            index_y = 2*m+2
            prediction[columns[index_x]] = x*4 # this is to recover the resolution to 256*256
            prediction[columns[index_y]] = y*4 # this is to recover the resolution to 256*256
        print(prediction)
        count = count + 1
        print(count)
        predictions.append(prediction)
    return predictions

if __name__ == '__main__': 

    s = 'build_HG' 
    csv_columns = ['File', 'ex_x', 'ex_y', 'en_x','en_y']
    predictions = get_predictions(s,csv_columns) 
    try: 
        with open('predictions_JWM.csv', 'w') as csvfile:
            writer = csv.DictWriter(csvfile, fieldnames=csv_columns)
            writer.writeheader() 
            for data in predictions: 
                writer.writerow(data) 
    except IOError: 
        print("I/O error")
