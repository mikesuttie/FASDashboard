# -*- coding: utf-8 -*-
import os
import numpy as np
#import random
from scipy.io import loadmat
import random
#from natsort import humansorted, ns  #TODO remove natsort from dependencies list
cwd = os.getcwd()

test_path = os.path.join(cwd, 'JWM') # directory for testing images
#test_list = humansorted(os.listdir(test_path))  



test_batch_sz = 2   ## batch size for testing

##RH
test_label_path = 'JWM'
test_coord_path = 'test_predictions'

## configuration papameters

params = {'dim_img': 256,
          'dim_label': 64,
          'batch_size': test_batch_sz,
          'shuffle': True,
          'flip_prob' : 0.5,
          'scale_range' : 0.2}

params_test = {'dim_img': 256,
          'dim_label': 64,
          'batch_size': test_batch_sz,
          'shuffle': False}

#partition = {'test': test_list}# IDs

def get_modelname(s):
    return 'models/{}.hdf5'.format(s)
