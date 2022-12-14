from typing import Optional, Union
from pydantic import Json

from app.models.core import IDModelMixin, CoreModel, DateTimeModelMixin
from app.models.user import UserPublic


class LandmarkBase(CoreModel):
    facescan_id: Optional[int]
    landmarksetcode: Optional[str]
    landmarks: Optional[str] #Json #It appears, the PostGres json type converts to str in SQL query. Also, see l.19


class LandmarkCreate(LandmarkBase):
    facescan_id: int
    landmarksetcode: str    
    algorithm: str
    computationstatus: str
    landmarks: str #Json 
    owner: int


class LandmarkInDB(IDModelMixin, DateTimeModelMixin, LandmarkBase):
    id: Optional[int]
    #facescan_id: Optional[int]
    #landmarksetcode: Optional[str]
    algorithm: Optional[str]
    computationstatus: Optional[str]
    #landmarks: Optional[Json]
    owner: int


class LandmarkPublic(IDModelMixin, LandmarkBase):
    owner: Union[int, UserPublic] # Type is either int or UserPublic
    
        
