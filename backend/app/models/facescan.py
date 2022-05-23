from datetime import date
from typing import Optional, Union
from enum import Enum
from app.models.core import IDModelMixin, CoreModel, DateTimeModelMixin
from app.models.user import UserPublic


class FacescanBase(CoreModel):
    patient_id: Optional[int]
    scan_date: Optional[date]
    imaging_modality: Optional[str]
    mesh: Optional[bytes]
    texture: Optional[bytes]


class FacescanCreate(FacescanBase):
    #patient_id: int
    scan_date: date
    imaging_modality: str
    #mesh: bytes
    #texture: bytes
    #owner: int


class FacescanInDB(IDModelMixin, DateTimeModelMixin, FacescanBase):
    id: Optional[int]
    patient_id: Optional[int]
    scan_date: Optional[date]
    imaging_modality: Optional[str]
    owner: int
    #mesh: Optional[bytes]
    #texture: Optional[bytes]


class FacescanPublic(IDModelMixin, FacescanBase):
    owner: Union[int, UserPublic] # Type is either int or UserPublic
    
    
class FacescanMeshInDB(IDModelMixin):
    mesh: bytes
    owner: Union[int, UserPublic] 


class FacescanTextureInDB(IDModelMixin):
    texture: bytes
    owner: Union[int, UserPublic] 

