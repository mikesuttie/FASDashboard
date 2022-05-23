from fastapi import HTTPException, status
from typing import List
from asyncpg.exceptions import ForeignKeyViolationError

from app.db.repositories.base import BaseRepository
from app.models.facescan import FacescanCreate, FacescanInDB, FacescanMeshInDB, FacescanTextureInDB
from app.models.user import UserInDB

from tempfile import NamedTemporaryFile # For workaround in spooling vtkxmlpolydata
import vtk

GET_FACESCANS_OF_PATIENT_QUERY = """
    SELECT id, patient_id, scan_date, imaging_modality, owner, created_at, updated_at
    FROM facescans
    WHERE owner = :owner AND patient_id = :patient_id;
"""

CREATE_FACESCAN_QUERY = """
    INSERT INTO facescans (patient_id, scan_date, imaging_modality, mesh, texture, owner)
    VALUES (:patient_id, :scan_date, :imaging_modality, :mesh, :texture, :owner)
    RETURNING id, patient_id, scan_date, imaging_modality, mesh, texture, owner, created_at, updated_at;
"""

UPLOAD_FACESCAN_MESH_QUERY = """
    UPDATE facescans
    SET mesh = :mesh
    WHERE owner = :owner AND id = :id
    RETURNING id, patient_id, scan_date, imaging_modality, owner;
"""

UPLOAD_FACESCAN_TEXTURE_QUERY = """
    UPDATE facescans
    SET texture = :texture
    WHERE owner = :owner AND id = :id
    RETURNING id, patient_id, scan_date, imaging_modality, owner;
""" 

GET_FACESCAN_BY_ID_QUERY = """
    SELECT id, patient_id, scan_date, imaging_modality, owner, created_at, updated_at
    FROM facescans
    WHERE owner = :owner AND id = :id;
"""

LIST_ALL_FACESCANS_OWNED_BY_USER_QUERY = """
    SELECT id, patient_id, scan_date, imaging_modality, owner, created_at, updated_at
    FROM facescans
    WHERE owner = :owner;
"""

DELETE_FACESCAN_BY_ID_QUERY = """
    DELETE FROM facescans
    WHERE owner = :owner AND id = :id
    RETURNING id;
"""

DOWNLOAD_FACESCAN_MESH_QUERY = """
    SELECT id, mesh, owner
    FROM facescans
    WHERE owner = :owner AND id = :id;
"""

DOWNLOAD_FACESCAN_TEXTURE_QUERY = """
    SELECT id, texture, owner
    FROM facescans
    WHERE owner = :owner AND id = :id;
"""


GET_PATIENT_AGE_OF_FACESCAN = """
    SELECT DATE_PART('day', (facescans.scan_date)::timestamp - (patients.dob)::timestamp )
    FROM facescans
    INNER JOIN patients ON patients.id = facescans.patient_id
    WHERE facescans.owner = :owner AND facescans.id = :id;

"""


class FacescansRepository(BaseRepository):
    """
    """
    
    async def get_all_facescans_for_patient(self, *, patient_id: int, requesting_user: UserInDB) -> List[FacescanInDB]:
        query_values = {}
        query_values['patient_id'] = patient_id
        query_values['owner'] = requesting_user.id
        facescans = await self.db.fetch_all(query=GET_FACESCANS_OF_PATIENT_QUERY, values=query_values)
        return [FacescanInDB(**facescan) for facescan in facescans]        
    
    
    async def create_facescan(self, *, new_facescan: FacescanCreate, patient_id: int, requesting_user: UserInDB) -> FacescanInDB:
        query_values = new_facescan.dict()
        try:
            facescan = await self.db.fetch_one(query=CREATE_FACESCAN_QUERY, values={**query_values, 
                "patient_id": patient_id,  "mesh": bytes("".encode()), "texture": bytes("".encode()), "owner": requesting_user.id})
        except (ForeignKeyViolationError):
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail="Patient ID does not exist (Foreign Key Violation Error).",
                headers={"WWW-Authenticate": "Bearer"},
            ) 
                        
        return FacescanInDB(**facescan)
        
        
    async def upload_facescan_mesh(self, *, facescan_id: int, meshfile, requesting_user: UserInDB) -> FacescanInDB:
        query_values = {}
        query_values['id'] = facescan_id
        query_values['mesh'] = await meshfile.read()
        query_values['owner'] = requesting_user.id        
        facescan = await self.db.fetch_one(query=UPLOAD_FACESCAN_MESH_QUERY, values=query_values)

        return FacescanInDB(**facescan)

    async def download_facescan_mesh(self, *, facescan_id: int, requesting_user: UserInDB) -> FacescanInDB:
        query_values = {}
        query_values['id'] = facescan_id
        query_values['owner'] = requesting_user.id        
        facescan = await self.db.fetch_one(query=DOWNLOAD_FACESCAN_MESH_QUERY, values=query_values)
        #TODO What to return if facescan is not a DB record, but none?
        return FacescanMeshInDB(**facescan)    

    async def upload_facescan_texture(self, *, facescan_id: int, texturefile, requesting_user: UserInDB) -> FacescanInDB:
        query_values = {}
        query_values['id'] = facescan_id
        query_values['texture'] = await texturefile.read()
        query_values['owner'] = requesting_user.id        
        facescan = await self.db.fetch_one(query=UPLOAD_FACESCAN_TEXTURE_QUERY, values=query_values)
        return FacescanInDB(**facescan)
    
    async def download_facescan_texture(self, *, facescan_id: int, requesting_user: UserInDB) -> FacescanInDB:
        query_values = {}
        query_values['id'] = facescan_id
        query_values['owner'] = requesting_user.id        
        facescan = await self.db.fetch_one(query=DOWNLOAD_FACESCAN_TEXTURE_QUERY, values=query_values)
        return FacescanTextureInDB(**facescan)

    
    async def get_facescan_by_id(self, *, id: int, requesting_user: UserInDB) -> FacescanInDB:
        facescan = await self.db.fetch_one(query=GET_FACESCAN_BY_ID_QUERY, values={"id": id, "owner": requesting_user.id})
        if not facescan:
            return None
        return FacescanInDB(**facescan)
        
        
    async def get_vtkxmlpolydata_mesh(self, *, id: int, requesting_user: UserInDB):
        facescanWaveFrontOBJ = await self.db.fetch_one(query=DOWNLOAD_FACESCAN_MESH_QUERY, values={"id": id, "owner": requesting_user.id})
        
        if not facescanWaveFrontOBJ:
            return None

        try:           
            wavefrontObjData = facescanWaveFrontOBJ[1]
        except IndexError:
            return None            
                
        with NamedTemporaryFile() as wavefrontObjTempfile:
            wavefrontObjTempfile.write(wavefrontObjData)
            wavefrontObjTempfile.flush()
    
            objReader = vtk.vtkOBJReader()
            objReader.SetFileName(str(wavefrontObjTempfile.name))
            objReader.Update()
            polydata = objReader.GetOutput()
           
            vtkPolydataTmpfile = NamedTemporaryFile()
            writer = vtk.vtkXMLPolyDataWriter()
            writer.SetFileName(vtkPolydataTmpfile.name)
            writer.SetInputData(polydata) 
            writer.SetDataModeToBinary()
            writer.Write()
            vtkPolydataTmpfile.flush()
            vtkPolydataTmpfile.seek(0)
            
            return vtkPolydataTmpfile.read()

           
    async def list_all_facescans_owned_by_user(self, *, requesting_user: UserInDB) -> List[FacescanInDB]:
        facescan_records = await self.db.fetch_all(
            query=LIST_ALL_FACESCANS_OWNED_BY_USER_QUERY, values={"owner": requesting_user.id}
        )
        return [FacescanInDB(**r) for r in facescan_records]


    async def delete_facescan_by_id(self, *, facescan: FacescanInDB, requesting_user: UserInDB) -> int:
        return await self.db.execute(query=DELETE_FACESCAN_BY_ID_QUERY, values={"id": facescan.id, "owner": requesting_user.id})


    async def get_patient_age_at_time_of_facescan(self, *, facescan_id: int, requesting_user: UserInDB) -> float:
        query_values = {}
        query_values['id'] = facescan_id
        query_values['owner'] = requesting_user.id  
        ageAtFacescanRecordingTime = await self.db.fetch_one(query=GET_PATIENT_AGE_OF_FACESCAN, values=query_values)
        
        if ageAtFacescanRecordingTime is not None:
            patientAgeInYears = ageAtFacescanRecordingTime[0] / 365.24
            return patientAgeInYears
            
        return None    
        
