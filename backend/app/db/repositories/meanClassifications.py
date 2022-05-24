from fastapi import HTTPException, status
from typing import List
from asyncpg.exceptions import ForeignKeyViolationError

from app.db.repositories.base import BaseRepository
from app.models.meanClassification import MeanClassificationCreate, MeanClassificationInDB
from app.models.user import UserInDB

CREATE_MEANCLASSIFICATION_RECORDS_BY_LANDMARK_ID_QUERY = """
    INSERT INTO meanclassificationresults (landmark_id, meanvalue, stderror, owner)
    VALUES (:landmark_id, :meanvalue, :stderror, :owner)
    RETURNING id, landmark_id, meanvalue, stderror, owner, created_at, updated_at;
"""

GET_MEANCLASSIFICATION_RECORD_BY_LANDMARK_ID_QUERY = """
    SELECT id, landmark_id, meanvalue, stderror, owner, created_at, updated_at;
    FROM meanclassificationresults
    WHERE owner = :owner AND landmark_id = :landmark_id;
"""

DELETE_MEANCLASSIFICATIONRECORD_BY_ID_QUERY = """
    DELETE FROM meanclassificationresults
    WHERE owner = :owner AND id = :id
    RETURNING id;
"""


class MeanClassificationRepository(BaseRepository):
    """
    """

    async def create_meanclassificationresult_record(self, *, new_meanclassificationresult: MeanClassificationCreate, requesting_user: UserInDB) -> MeanClassificationInDB:
        query_values = new_meanClassificationResult.dict()
        try:
            meanClassificationRecord = await self.db.fetch_one(query=CREATE_MEANCLASSIFICATION_RECORDS_BY_LANDMARK_ID_QUERY, values={**query_values, 
                "landmark_id": new_meanClassificationResult.landmark_id,  
                "meanvalue": new_meanClassificationResult.meanvalue, 
                "stderror": new_meanClassificationResult.stderror, 
                "owner": requesting_user.id})
        except (ForeignKeyViolationError):
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail="Cannot insert meanclassfication records, as landmark ID does not exist: Foreign Key Violation Error.",
                headers={"WWW-Authenticate": "Bearer"},
            )                         
        return MeanClassificationInDB(**meanClassificationRecord)

    
    async def get_meanClassificationRecord(self, *, landmark_id: int, requesting_user: UserInDB) -> List[MeanClassificationInDB]:
        query_values = {}
        query_values['landmark_id'] = landmark_id
        query_values['owner'] = requesting_user.id
        meanClassificationRecords = await self.db.fetch_all(query=GET_MEANCLASSIFICATION_RECORD_BY_LANDMARK_ID_QUERY, values=query_values)
        return [MeanClassificationInDB(**meanClassificationRecord) for meanClassificationRecord in meanClassificationRecords]
    
    
    async def delete_meanclassification_by_id(self, *, meanclassificationRecord: MeanClassificationInDB, requesting_user: UserInDB) -> int:
        return await self.db.execute(query=DELETE_MEANCLASSIFICATIONRECORD_BY_ID_QUERY, values={"id": meanclassificationRecord.id, "owner": requesting_user.id})



