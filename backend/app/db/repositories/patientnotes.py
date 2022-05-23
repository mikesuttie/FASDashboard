from fastapi import HTTPException, status
from typing import List
from asyncpg.exceptions import ForeignKeyViolationError

from app.db.repositories.base import BaseRepository
from app.models.patientnote import PatientnoteCreate, PatientnoteInDB
from app.models.user import UserInDB


GET_ALL_PATIENTNOTES_OWNED_BY_USER_QUERY = """
    SELECT id, patient_id, note, owner, created_at, updated_at
    FROM patientnotes
    WHERE owner = :owner;
"""

GET_PATIENTNOTE_OF_PATIENT_QUERY = """
    SELECT * 
    FROM patientnotes
    WHERE patient_id = :patient_id AND owner = :owner;
"""

CREATE_PATIENTNOTE_QUERY = """
    INSERT INTO patientnotes (patient_id, note, owner)
    VALUES (:patient_id, :note, :owner)
    RETURNING id, patient_id, note, owner, created_at, updated_at;
"""

UPDATE_PATIENTNOTE_BY_ID_QUERY = """
    UPDATE patientnotes
    SET note = :note
    WHERE id = :id AND owner = :owner
    RETURNING id, patient_id, note, owner, created_at, updated_at;
"""

DELETE_PATIENTNOTE_BY_ID_QUERY = """
    DELETE FROM patientnotes
    WHERE id = :id
    RETURNING id;
"""


class PatientnotesRepository(BaseRepository):
    """
    """

    async def get_all_patientnotes_owned_by_user(self, *, requesting_user: UserInDB) -> List[PatientnoteInDB]:
        patientnote_records = await self.db.fetch_all(
            query=GET_ALL_PATIENTNOTES_OWNED_BY_USER_QUERY, values={"owner": requesting_user.id}
        )
        return [PatientnoteInDB(**r) for r in patientnote_records]

    
    async def get_all_patientnotes_for_patient(self, *, requesting_user: UserInDB, patient_id) -> List[PatientnoteInDB]:
        query_values = {}
        query_values['patient_id'] = patient_id
        query_values['owner'] = requesting_user.id
        patientnotes = await self.db.fetch_all(query=GET_PATIENTNOTE_OF_PATIENT_QUERY, values=query_values)
        return [PatientnoteInDB(**patientnote) for patientnote in patientnotes]
    
    
    async def create_patientnote(self, *, new_patientnote: PatientnoteCreate, requesting_user: UserInDB) -> PatientnoteInDB:
        query_values = new_patientnote.dict()
        try:
            patientnote = await self.db.fetch_one(query=CREATE_PATIENTNOTE_QUERY, values={**query_values, "owner": requesting_user.id})
        except (ForeignKeyViolationError):
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail="Patient ID does not exist (Foreign Key Violation Error).",
                headers={"WWW-Authenticate": "Bearer"},
            ) 
                        
        return PatientnoteInDB(**patientnote)
        
        
    async def update_patientnote_by_id(self, *, patientnote_id, patientnote, requesting_user: UserInDB) -> PatientnoteInDB:
        query_values = {}
        query_values['id'] = patientnote_id
        query_values['note'] = note
        query_values['owner'] = requesting_user
        patientnote = await self.db.fetch_one(query=UPDATE_PATIENTNOTE_BY_ID_QUERY, values=query_values)
        return PatientnoteInDB(**patientnote)    
           

    async def delete_patientnote_by_id(self, *, patientnote: PatientnoteInDB) -> int:
        return await self.db.execute(query=DELETE_PATIENTNOTE_BY_ID_QUERY, values={"id": patientnote.id})
        
        
