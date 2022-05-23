from fastapi import HTTPException, status
from typing import List
from asyncpg.exceptions import ForeignKeyViolationError

from app.db.repositories.base import BaseRepository
from app.models.signature import SignatureCreate, SignatureInDB, SignatureHeatmapMeshInDB
from app.models.user import UserInDB

from tempfile import NamedTemporaryFile # For workaround in spooling vtkxmlpolydata
import vtk
import json

CREATE_SIGNATURE_RECORDS_BY_LANDMARK_ID_QUERY = """
    INSERT INTO signatures (landmark_id, modelname, facialregion, owner)
    VALUES (:landmark_id, :modelname, :facialregion, :principal_components, :owner)
    RETURNING id, landmark_id, modelname, facialregion, principal_components, owner, created_at, updated_at;
"""

GET_SIGNATURE_RECORDS_BY_LANDMARK_ID_QUERY = """
    SELECT id, landmark_id, modelname, owner, created_at, updated_at
    FROM signatures
    WHERE owner = :owner AND landmark_id = :landmark_id;
"""

UPLOAD_HEATMAP_MESH_QUERY = """
    UPDATE signatures
    SET heatmap = :heatmap
    WHERE owner = :owner AND id = :id
    RETURNING id, landmark_id, modelname, owner;
"""

DOWNLOAD_HEATMAP_MESH_QUERY = """
    SELECT id, heatmap, owner
    FROM signatures
    WHERE owner = :owner AND id = :id;
"""

DELETE_SIGNATURE_BY_ID_QUERY = """
    DELETE FROM signatures
    WHERE owner = :owner AND id = :id
    RETURNING id;
"""


class SignatureRepository(BaseRepository):
    """
    """

    async def create_signature_record(self, *, new_signature: SignatureCreate, requesting_user: UserInDB) -> SignatureInDB:
        query_values = new_signature.dict()
        try:
            if query_values.modelname is None or query_values.modelname == "":
                query_values.modelname = "UNSPECIFIED" # Value for foreign key prepopulated in app/db/migrations/versions
            signature = await self.db.fetch_one(query=CREATE_SIGNATURE_RECORDS_BY_LANDMARK_ID_QUERY, values={**query_values, 
                "landmark_id": new_signature.landmark_id,  
                "facialregion": new_signature.facialRegion , 
                "principal_components" : json.dumps(new_signature.principalComponents),
                "heatmap": bytes("".encode()), 
                "owner": requesting_user.id})
        except (ForeignKeyViolationError):
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail="Landmark ID OR Facial Region: Foreign Key Violation Error.",
                headers={"WWW-Authenticate": "Bearer"},
            )                         
        return SignatureInDB(**signature)

    
    async def get_all_signature_records_for_landmark_record(self, *, landmark_id: int, requesting_user: UserInDB) -> List[SignatureInDB]:
        query_values = {}
        query_values['landmark_id'] = landmark_id
        query_values['owner'] = requesting_user.id
        signatures = await self.db.fetch_all(query=GET_SIGNATURE_RECORDS_BY_LANDMARK_ID_QUERY, values=query_values)
        return [SignatureInDB(**signature) for signature in signatures]
    

    async def upload_heatmap_mesh(self, *, signature: SignatureInDB, meshfile, requesting_user: UserInDB) -> SignatureInDB:
        query_values = {}
        query_values['id'] = signature.id
        query_values['heatmap'] = await meshfile.read()
        query_values['owner'] = requesting_user.id        
        signature = await self.db.fetch_one(query=UPLOAD_HEATMAP_MESH_QUERY, values=query_values)
        return SignatureInDB(**signature)


    async def download_heatmap_mesh(self, *, signature_id: int, requesting_user: UserInDB) -> SignatureHeatmapMeshInDB:
        query_values = {}
        query_values['id'] = signature_id
        query_values['owner'] = requesting_user.id        
        facescan = await self.db.fetch_one(query=DOWNLOAD_HEATMAP_MESH_QUERY, values=query_values)
        return FacescanMeshInDB(**facescan)    


    async def delete_signature_by_id(self, *, signature: SignatureInDB, requesting_user: UserInDB) -> int:
        return await self.db.execute(query=DELETE_SIGNATURE_BY_ID_QUERY, values={"id": signature.id, "owner": requesting_user.id})



