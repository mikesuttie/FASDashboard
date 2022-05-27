from fastapi import HTTPException, status
from typing import List
from asyncpg.exceptions import ForeignKeyViolationError

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
    WHERE owner = :owner;
"""

class PatientsRepository(BaseRepository):
    """
    """
    async def create_patient(self, *, new_patient: PatientCreate, requesting_user: UserInDB) -> PatientInDB:
        query_values = new_patient.dict()
        query_values['owner'] = requesting_user.id
        try:
            patient = await self.db.fetch_one(query=CREATE_PATIENT_QUERY, values=query_values)
        except ForeignKeyViolationError:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND, detail="Ethnicity code of new patient unknown (Foreign key violation error).",
            )        
        
        return PatientInDB(**patient)
        
        
    async def get_all_patients(self, *, requesting_user: UserInDB) -> List[PatientInDB]:
        allpatients = await self.db.fetch_all(query=GET_ALL_PATIENTS_QUERY, values={'owner':requesting_user.id})
        return [PatientInDB(**patient) for patient in allpatients]
    
