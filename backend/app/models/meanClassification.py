from datetime import date
from typing import Optional, Union
from enum import Enum
from app.models.core import IDModelMixin, CoreModel, DateTimeModelMixin
from app.models.user import UserPublic
from pydantic import Json

class MeanClassificationBase(CoreModel):
    signature_id: Optional[int]
    meanvalue: Optional[float]
    stderror: Optional[float]    


class MeanClassificationCreate(MeanClassificationBase):
    signature_id: int
    meanvalue: float
    stderror: float    


class MeanClassificationInDB(IDModelMixin, DateTimeModelMixin, MeanClassificationBase):
    id: Optional[int]
    signature_id: Optional[int]
    meanvalue: Optional[float]
    stderror: Optional[float]    
    owner: int


class MeanClassificationPublic(IDModelMixin, MeanClassificationBase):
    owner: Union[int, UserPublic] # Type is either int or UserPublic
    
