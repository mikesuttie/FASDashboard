#from datetime import date, datetime
import datetime
from typing import Optional, List
from enum import Enum
from app.models.core import IDModelMixin, CoreModel

class PatientSex(str, Enum):
    male = "male"
    female = "female"

class PatientBase(CoreModel):
    """
    
    """
    name: Optional[str]
    surname: Optional[str]
    ethnicity: Optional[str]
    dob: Optional[datetime.date]
    sex: Optional[PatientSex]

class PatientCreate(PatientBase):
    name: str
    surname: str
    ethnicity: str
    dob: datetime.date
    sex: PatientSex
    owner: int    

class PatientUpdate(PatientBase):
    name: Optional[str]
    surname: Optional[str]
    ethnicity: Optional[str]
    dob: Optional[datetime.date]
    sex: Optional[PatientSex]
    owner: Optional[int]

class PatientInDB(IDModelMixin, PatientBase):
    name: str
    surname: str
    ethnicity: str
    dob: datetime.date
    sex: PatientSex
    owner: int    

class PatientPublic(IDModelMixin, PatientBase):
    owner: Union[int, UserPublic] # Type is either int or UserPublic
