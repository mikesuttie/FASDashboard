from typing import Optional
from pydantic import Json

from app.models.core import CoreModel


class LandmarksetBase(CoreModel):
    """
    """
    code: Optional[str]
    landmarklist: Optional[Json]

    
class LandmarksetCreate(LandmarksetBase):
    code: str
    landmarklist: Json


class LandmarksetUpdate(LandmarksetBase):
    code: Optional[str]
    landmarklist: Optional[Json]


class LandmarksetInDB(LandmarksetBase):
    code: str
    landmarklist: Json
    
class LandmarksetPublic(LandmarksetBase):
    pass
