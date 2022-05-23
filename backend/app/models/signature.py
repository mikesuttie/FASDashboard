from datetime import date
from typing import Optional, Union
from enum import Enum
from app.models.core import IDModelMixin, CoreModel, DateTimeModelMixin
from app.models.user import UserPublic
from pydantic import Json

class SignatureBase(CoreModel):
    landmark_id: Optional[int]
    modelname: Optional[str]
    facialregion: Optional[str]    
    principal_components: Optional[Json]
    heatmap: Optional[bytes]


class SignatureCreate(SignatureBase):
    landmark_id: int
    modelname: str
    facialregion: str
    principal_components: Json


class SignatureInDB(IDModelMixin, DateTimeModelMixin, SignatureBase):
    id: Optional[int]
    landmark_id: Optional[int]
    modelname: Optional[str]
    facialregion: Optional[str]        
    principal_components: Optional[Json]    
    owner: int


class SignaturePublic(IDModelMixin, SignatureBase):
    owner: Union[int, UserPublic] # Type is either int or UserPublic
    
    
class SignatureHeatmapMeshInDB(IDModelMixin):
    heatmap: bytes
    owner: Union[int, UserPublic] 


