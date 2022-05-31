from datetime import date
from typing import Optional, Union
from enum import Enum
from app.models.core import IDModelMixin, CoreModel, DateTimeModelMixin
from app.models.user import UserPublic
from pydantic import Json

class MeanClassificationBase(CoreModel):
    landmark_id: Optional[int]
    modelname: Optional[str]
    facialregion: Optional[str]    
    meanvalue: Optional[float]
    stderror: Optional[float]    
    neg_class: Optional[str]    
    pos_class: Optional[str]    


class MeanClassificationCreate(MeanClassificationBase):
    landmark_id: int
    modelname: str
    facialregion: str
    meanvalue: float
    stderror: float    
    neg_class: str
    pos_class: str


class MeanClassificationInDB(IDModelMixin, DateTimeModelMixin, MeanClassificationBase):
    id: Optional[int]
    owner: int


class MeanClassificationPublic(IDModelMixin, MeanClassificationBase):
    owner: Union[int, UserPublic] # Type is either int or UserPublic
    
