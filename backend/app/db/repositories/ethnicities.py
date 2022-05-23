from app.db.repositories.base import BaseRepository
from app.models.ethnicity import EthnicityCreate, EthnicityUpdate, EthnicityInDB

from fastapi import HTTPException, status

import asyncpg.exceptions

CREATE_ETHNICITY_QUERY = """
    INSERT INTO ethnicities (code, name, country)
    VALUES (:code, :name, :country)
    RETURNING code, name, country;
"""

GET_ALL_ETHNICITIES_QUERY = """
    SELECT * 
    FROM ethnicities;   
"""

class EthnicitiesRepository(BaseRepository):
    """
    """
    async def create_ethnicity(self, *, new_ethnicity: EthnicityCreate) -> EthnicityInDB:
        query_values = new_ethnicity.dict()
        try:
            ethnicity = await self.db.fetch_one(query=CREATE_ETHNICITY_QUERY, values=query_values)
        except (asyncpg.exceptions.UniqueViolationError):        
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail="Ethnicity code already exists.",
                #headers={"WWW-Authenticate": "Bearer"},
            ) 
        
        return EthnicityInDB(**ethnicity)
        
        
    async def get_all_ethnicities(self) -> EthnicityInDB:
        allethnicities = await self.db.fetch_all(query=GET_ALL_ETHNICITIES_QUERY)
        return [EthnicityInDB(**ethnicity) for ethnicity in allethnicities]
        
