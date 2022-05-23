# -*- coding: utf-8 -*-
from keras.layers import (
        Input,
        Dense,
        Lambda,Concatenate,
        Flatten,
        Conv2D,
        MaxPooling2D,
        UpSampling2D,
        Activation,Add,Multiply)
from keras.models import Model
from keras.layers import Reshape, GlobalAveragePooling2D, Conv2DTranspose, RepeatVector,Permute, GlobalMaxPooling2D
from keras.layers.merge import concatenate
from keras.layers.normalization import BatchNormalization
from keras.regularizers import l2
from keras.applications import VGG16
from keras.applications.resnet50 import ResNet50
from keras.applications.mobilenetv2 import MobileNetV2
import keras.backend as K
from keras.optimizers import Adam,SGD
from keras.losses import binary_crossentropy, mean_squared_error, cosine_proximity
from keras.losses import categorical_crossentropy as logloss
from keras_vggface.vggface import VGGFace

from cnnPrediction.hg_blocks import create_hourglass_network, euclidean_loss,bottleneck_block,bottleneck_mobile, bottleneck_mobile_v2, vgg_block, SE_block
# from vggface2 import Vggface2_ResNet50


def Diceloss(y_true,y_pred):
    y_true_f = K.flatten(y_true)
    y_pred_f = K.flatten(y_pred)
    Intersection_area = K.sum(y_true_f*y_pred_f)
    Union = 1.0*(K.sum(y_true_f) + K.sum(y_pred_f) - Intersection_area)
    return  1-((Intersection_area+ 0.1)/(Union+0.1))

def mseloss(y_true,y_pred):
    y_true_f = K.flatten(y_true)
    y_pred_f = K.flatten(y_pred)
    return K.mean(K.square(y_pred - y_true), axis=-1)


weight_decay = 1e-4

def _conv_bn_relu2D(nb_filter, kernel_dim1, kernel_dim2, strides = (1,1)):
    def f(input):
        conv_a = Conv2D(nb_filter, (kernel_dim1, kernel_dim2), strides= strides,
                               kernel_initializer = 'orthogonal', padding = 'same', 
                               kernel_regularizer = l2(weight_decay),
                               bias_regularizer = l2(weight_decay))(input)
        norm_a = BatchNormalization()(conv_a)
        act_a = Activation(activation = 'relu')(norm_a)
        conv_b = Conv2D(nb_filter, (kernel_dim1, kernel_dim2), strides= strides,
                               kernel_initializer = 'orthogonal', padding = 'same', 
                               kernel_regularizer = l2(weight_decay),
                               bias_regularizer = l2(weight_decay))(act_a)
        norm_b = BatchNormalization()(conv_b)
        act_b = Activation(activation = 'relu')(norm_b)
        return act_b
    return f


    # =================================================================
    # ============================Hourglass network =====================================
def build_HG(num_classes=2,num_stacks=1, num_channels=128,inres=(256,256), outres=(64,64)):
    
    bottleneck = bottleneck_block
    model = create_hourglass_network(num_classes, num_stacks,num_channels,inres, outres, bottleneck)
    
    # show model summary and layer name
    
    #model.summary()
    return model


def tf_hm(input_dim, output_dim):
    baseModel = VGGFace(include_top=False, model='resnet50', weights=None, input_tensor=Input(shape=input_dim))
    #baseModel.summary()    
    #last_layer = baseModel.get_layer('activation_49').output
    last_layer = baseModel.get_layer('activation_49').output
    # basemodel.summary()
    # last_layer = BatchNormalization()(last_layer)
    x = Conv2DTranspose(256, (4, 4), strides=(2, 2), kernel_initializer='he_normal', padding='same',
                        kernel_regularizer=l2(weight_decay),
                        )(last_layer)
    x = BatchNormalization()(x)
    x = Activation(activation='relu')(x)
    
    x = Conv2DTranspose(256, (4, 4), strides=(2, 2), kernel_initializer='he_normal', padding='same',
                        kernel_regularizer=l2(weight_decay),
                        )(x)
    x = BatchNormalization()(x)
    x = Activation(activation='relu')(x)
    
    x = Conv2DTranspose(256, (4, 4), strides=(2, 2), kernel_initializer='he_normal', padding='same',
                        kernel_regularizer=l2(weight_decay),
                        )(x)
    x = BatchNormalization()(x)
    x = Activation(activation='relu')(x)
    
    out = Conv2D(output_dim, (1, 1), padding="same", \
                 kernel_initializer="he_normal", activation="linear", name="output")(x)
    
    model = Model(inputs=baseModel.input, outputs=out)
    
    opt = Adam(lr=0.001, beta_1=0.9, beta_2=0.999, epsilon=1e-08, decay=0.0)
    model.compile(optimizer=opt, loss='mean_squared_error', metrics=['mse', 'acc'])
    #model.summary()
    return model


