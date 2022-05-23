from app.db.repositories.base import BaseRepository
from app.models.landmarkset import LandmarksetCreate, LandmarksetUpdate, LandmarksetInDB

from fastapi import HTTPException, status

import asyncpg.exceptions


# TODO none of these 3 query strings work as expected to write json data into column. 
# Find out how to fix this.

tmp_CREATE_LANDMARKSET_QUERY = """
    INSERT INTO landmarksets
    SELECT code, landmarklist
    FROM json_populate_record (NULL::landmarksets,
        '{
        "code": :code,
        "landmarklist": :landmarklist
        }'
    )
    
    """
"""    
    SELECT landmarklist
    FROM json_populate_record (NULL::landmarksets,
        '["eye_left","eye_right", "mouth_centre"]'
    )
    RETURNING code, landmarkset;

"""

tmp2_CREATE_LANDMARKSET_QUERY = """
    INSERT INTO landmarksets (code, landmarkset)
    VALUES (:code, :landmarkset)
    RETURNING code, landmarkset;
"""

CREATE_LANDMARKSET_QUERY = """
    INSERT INTO landmarksets (code, landmarklist)
    VALUES (:code, :landmarklist)
    RETURNING code,landmarklist;
"""

"""
    FROM json_populate_record (NULL::landmarksets,
        '{
        "code": "test:code",
        "landmarklist": "test:landmarklist"
        }'
    );
"""




GET_ALL_LANDMARKSETS_QUERY = """
    SELECT * 
    FROM landmarksets;   
"""

class LandmarksetsRepository(BaseRepository):
    """
    """
    async def create_landmarkset(self, *, new_landmarkset: LandmarksetCreate) -> LandmarksetInDB:
        query_values = new_landmarkset.dict()
        try:
            landmarkset = await self.db.fetch_one(query=CREATE_LANDMARKSET_QUERY, values=query_values)
        except (asyncpg.exceptions.UniqueViolationError):        
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail="Landmarkset code already exists.",
                #headers={"WWW-Authenticate": "Bearer"},
            ) 
        
        return LandmarksetInDB(**landmarkset)
        
        
    async def get_all_landmarksets(self) -> LandmarksetInDB:
        alllandmarksets = await self.db.fetch_all(query=GET_ALL_LANDMARKSETS_QUERY)
        return [LandmarksetInDB(**landmarkset) for landmarkset in alllandmarksets]
        
