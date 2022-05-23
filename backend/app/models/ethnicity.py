from typing import Optional
from app.models.core import CoreModel


class EthnicityBase(CoreModel):
    """
    """
    code: Optional[str]
    name: Optional[str]
    country: Optional[str]

    
class EthnicityCreate(EthnicityBase):
    code: str
    name: str
    country: str


class EthnicityUpdate(EthnicityBase):
    code: Optional[str]
    name: Optional[str]
    country: Optional[str]


class EthnicityInDB(EthnicityBase):
    code: str
    name: str
    country: str
    
class EthnicityPublic(EthnicityBase):
    pass
