from typing import List

from app.db.repositories.base import BaseRepository
from app.models.patient import PatientCreate, PatientUpdate, PatientInDB
from app.models.user import UserInDB

CREATE_PATIENT_QUERY = """
    INSERT INTO patients (name, surname, dob, ethnicity, sex, owner)
    VALUES (:name, :surname, :dob, :ethnicity, :sex, :owner)
    RETURNING id, name, surname, dob, ethnicity, sex, owner, created_at, updated_at;
"""

GET_ALL_PATIENTS_QUERY = """
    SELECT * 
    FROM patients
    WHERE id = :id AND owner = :owner;
"""

class PatientsRepository(BaseRepository):
    """
    """
    async def create_patient(self, *, new_patient: PatientCreate, requesting_user: UserInDB) -> PatientInDB:
        query_values = new_patient.dict()
        query_values['owner'] = requesting_user.id
        patient = await self.db.fetch_one(query=CREATE_PATIENT_QUERY, values=query_values)
        return PatientInDB(**patient)
        
        
    async def get_all_patients(self, *, requesting_user: UserInDB) -> List[PatientInDB]:
        query_values['owner'] = requesting_user.id    
        allpatients = await self.db.fetch_all(query=GET_ALL_PATIENTS_QUERY)
        return [PatientInDB(**patient) for patient in allpatients]
    
