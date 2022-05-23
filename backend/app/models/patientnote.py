from typing import Optional, Union
from app.models.core import IDModelMixin, CoreModel, DateTimeModelMixin
from app.models.user import UserPublic


class PatientnoteBase(CoreModel):
    patient_id: Optional[int]
    note: Optional[str]


class PatientnoteCreate(PatientnoteBase):
    patient_id: int
    note: str
    owner: int


class PatientnoteInDB(IDModelMixin, DateTimeModelMixin, PatientnoteBase):
    id: Optional[int]
    patient_id: Optional[int]
    note: Optional[str]
    owner: int


class PatientnotePublic(IDModelMixin, PatientnoteBase):
    owner: Union[int, UserPublic] # Type is either int or UserPublic
    
        
