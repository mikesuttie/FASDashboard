from fastapi import HTTPException, status
from typing import List
from asyncpg.exceptions import ForeignKeyViolationError

from app.db.repositories.base import BaseRepository
from app.models.signature import SignatureCreate, SignatureInDB, SignatureHeatmapMeshInDB
from app.models.user import UserInDB

from tempfile import NamedTemporaryFile # For workaround in spooling vtkxmlpolydata
import asyncpg.exceptions
import httpx
import vtk
import json

CREATE_SIGNATURE_RECORDS_BY_LANDMARK_ID_QUERY = """
    INSERT INTO signatures (landmark_id, modelname, facialregion, principal_components, owner)
    VALUES (:landmark_id, :modelname, :facialregion, :principal_components, :owner)
    RETURNING id, landmark_id, modelname, facialregion, principal_components, owner, created_at, updated_at;
"""

CREATE_SIGNATURE_WITH_HEATMAPDATA_RECORD_BY_LANDMARK_ID_QUERY = """
    INSERT INTO signatures (landmark_id, modelname, facialregion, principal_components, heatmap, owner)
    VALUES (:landmark_id, :modelname, :facialregion, :principal_components, :heatmap, :owner)
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
    WHERE o waitwner = :owner AND id = :id
    RETURNING id;
"""

# 4 Queries fetching data required for signature computation on processing server
GET_PATIENT_AGE_IN_DAYS_BY_LANDMARK_ID = """
    SELECT DATE_PART('day', (facescans.scan_date)::timestamp - (patients.dob)::timestamp )
    FROM facescans
    INNER JOIN landmarks
    ON facescans.id = landmarks.facescan_id
    INNER JOIN patients 
    ON patients.id = facescans.patient_id    
    WHERE landmarks.owner = :owner AND landmarks.id = :landmark_id;
"""

GET_PATIENT_SEX_BY_LANDMARK_ID_REMOVEME = """
    SELECT sex 
    FROM patients
    INNER JOIN facescans
    ON patient.id = facescans.patient_id
    INNER JOIN landmarks
    ON facescan.id = landmarks.facescan_id
    WHERE landmarks.owner = :owner AND landmarks.id = :landmark_id;
"""


GET_PATIENT_SEX_BY_LANDMARK_ID = """
    SELECT patients.sex 
    FROM facescans
    INNER JOIN landmarks
    ON facescans.id = landmarks.facescan_id
    INNER JOIN patients 
    ON patients.id = facescans.patient_id    
    WHERE landmarks.owner = :owner AND landmarks.id = :landmark_id;
"""

GET_FACEMESH_BY_LANDMARK_ID = """
    SELECT mesh
    FROM facescans
    INNER JOIN landmarks
    ON facescans.id = landmarks.facescan_id
    WHERE landmarks.owner = :owner AND landmarks.id = :landmark_id;    
"""

GET_LANDMARKSTRING_ONLY_BY_LANDMARK_ID_QUERY = """
    SELECT landmarks
    FROM landmarks
    WHERE id = :landmark_id AND owner = :owner;
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
                "landmark_id": new_siprincipal_componentsgnature.landmark_id,  
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
        
        
    async def compute_signature(self, *, landmark_id: int, signaturemodel_name: str, facialregion_code: str, requesting_user: UserInDB) -> int: #TODO: return type:     SignatureInDB:
        with httpx.Client() as client:    
        
            # 1) Check if signature model / facial region are available.
            # TODO        
                        
            # 2) Fetch landmarks, and corresponding mesh, sex and age
            try:
                patientAgeInDays = await self.db.fetch_one(query=GET_PATIENT_AGE_IN_DAYS_BY_LANDMARK_ID,       values={"landmark_id": landmark_id, "owner": requesting_user.id})
                patientSexInDB   = await self.db.fetch_one(query=GET_PATIENT_SEX_BY_LANDMARK_ID,               values={"landmark_id": landmark_id, "owner": requesting_user.id})        
                faceMeshInDB     = await self.db.fetch_one(query=GET_FACEMESH_BY_LANDMARK_ID,                  values={"landmark_id": landmark_id, "owner": requesting_user.id})         
                landmarksInDB    = await self.db.fetch_one(query=GET_LANDMARKSTRING_ONLY_BY_LANDMARK_ID_QUERY, values={"landmark_id": landmark_id, "owner": requesting_user.id})         
            
            except (ForeignKeyViolationError) as err:
                raise HTTPException(
                    status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                    detail="Landmark principal_componentsID OR Facial Region: Foreign Key Violation Error: " + err.message,
                    headers={"WWW-Authenticate": "Bearer"},
                )          
            try:            
                landmarksInDB = json.loads(landmarksInDB[0])["consensus"]
            except:                
                raise HTTPException(
                    status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                    detail="Consensus landmarks not available in database!",
                    headers={"WWW-Authenticate": "Bearer"},
                )                            
            #print("patientAgeInDays[0] , patientSexInDB[0], len(faceMeshInDB[0]),landmarksInDB: ",    patientAgeInDays[0] , patientSexInDB[0], len(faceMeshInDB[0]),landmarksInDB)
                
            # 3) Get processingToken and transfer mesh and landmarks to processing server, and 
            
            try:
                # r = httpx.post("http://landmarkingserver:7998/detectLandmarks/", files=headGeometryData, timeout=100.0)                
                processingserver_response = client.get("http://facescreenprocessor:34568/faceScreen/processor/processingToken/", timeout=1.0) 
                #TODO check processingToken is valid, or return exception
            except httpx.ConnectError:
                print("Getting processing token: Connection refused to processingserver!")
                raise HTTPException(
                    status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                    detail="Processing server refused to issue processing token!",
                    headers={"WWW-Authenticate": "Bearer"},
                )
            
            processingToken = processingserver_response.text
            print("Processing server issued processing token: " , processingToken)

            # Upload landmarks (POST): curl -H "Content-Type: application/json" --data @JWM6314_5-MAR-2014.json http://localhost:34568/faceScreen/processor/landmarks?processingToken=2^&ethnicityCode=CAUC
            try:
                #TODO select ethnicity through API (see #1)
                processingserver_response = client.post("http://facescreenprocessor:34568/faceScreen/processor/landmarks", timeout=1.0, params = {'processingToken': processingToken, 'ethnicityCode': 'CAUC'}, json=landmarksInDB)                  
            except (httpx.ConnectError,httpx.ReadTimeout) as err:
                print("Transferring landmarks: Connection refused to processingserver!", type(err), err.message)
                raise HTTPException(
                    status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                    detail="Processing server refused to accept landmarks data: " + err.message,
                    headers={"WWW-Authenticate": "Bearer"},
                )

            print("UPLOADED LANDMARKS TO PROCESSNIG SERVER")

            # Upload mesh (PUT): curl -T JWM6314_5-MAR-2014.obj http://localhost:34568/faceScreen/processor/objFile?processingToken=2
            try:
                processingserver_response = client.put("http://facescreenprocessor:34568/faceScreen/processor/objFile?processingToken=" + processingToken , timeout=3.0, data=faceMeshInDB[0])
            except httpx.ConnectError:
                print("Uploading mesh: Connection refused to processingserver!")      
                raise HTTPException(
                    status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                    detail="Processing server refused to accept face mesh data!",
                    headers={"WWW-Authenticate": "Bearer"},
                )
            except httpx.ReadTimeout:
                print("Computing heatmap: Processingserver exceeded reading time. Try again!")
                raise HTTPException(
                    status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                    detail="Timeout error when transferring face mesh data to processing server!",
                    headers={"WWW-Authenticate": "Bearer"},
                )
          
            # 4) Invoke computation on processing server and forward result: ok/error
            #       e.g.: GET http://localhost:34568/faceScreen/processor/computeHeatmap?processingToken=2^&subjectAge=12
            subjectAge = patientAgeInDays[0] / 365.24

            try:
                processingserver_response = client.get("http://facescreenprocessor:34568/faceScreen/processor/computeHeatmap", timeout=10.0, params = {'processingToken': processingToken, 'subjectAge': subjectAge})
            except httpx.ConnectError:
                print("Computing heatmap: Connection refused to processingserver!")
            except httpx.ReadTimeout:
                print("Computing heatmap: Processingserver exceeded reading time. Try again!")
                raise HTTPException(
                    status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                    detail="Request to processing server timed out while computing heatmap!",
                    headers={"WWW-Authenticate": "Bearer"},
                )
            print("COMPUTED HEATMAP")
            

            # Retrieve heatmap data from processing server
            try:
                processingserver_response = client.get("http://facescreenprocessor:34568/faceScreen/processor/heatmapPolyData?processingToken=" + processingToken , timeout=5.0)
            except httpx.ConnectError:
                print("Getting heatmapPolyData: Connection refused to processingserver!")
            except httpx.ReadTimeout:
                print("Computing heatmap: Processingserver exceeded reading time. Try again!")
                raise HTTPException(
                    status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                    detail="Request to processing server timed out while loading heatmap result!",
                    headers={"WWW-Authenticate": "Bearer"},
                )            
            print("Size of heatmap data: " , len(processingserver_response.content))
        
        # 5) If ok, write heatmap result to new DB record, i.e., create signature record and write heatmap
       
        try:
            new_signature_record = await self.db.fetch_one(query=CREATE_SIGNATURE_WITH_HEATMAPDATA_RECORD_BY_LANDMARK_ID_QUERY, values={#**query_values, 
                "modelname" : "UNSPECIFIED",
                "landmark_id": landmark_id,  
                "facialregion": "FACE", 
                "principal_components" : "[]",
                "heatmap": processingserver_response.content, #bytes((processingserver_response.text).encode()), 
                "owner": requesting_user.id})
        except (ForeignKeyViolationError):  #TODO this shouldn't happen at this stage - Check before contacting processingServer!
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail="Landmark ID OR Facial Region: Foreign Key Violation Error.",
                headers={"WWW-Authenticate": "Bearer"},
            )                         

        try:
             new_signature_id = new_signature_record[0]
        except:
                raise HTTPException(
                    status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                    detail="Computed heatmap could not be inserted into database!",
                    headers={"WWW-Authenticate": "Bearer"},
                )                    

        return new_signature_id
        
    
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
        heatmapData = await self.db.fetch_one(query=DOWNLOAD_HEATMAP_MESH_QUERY, values=query_values)
        return SignatureHeatmapMeshInDB(**heatmapData)


    async def delete_signature_by_id(self, *, signature: SignatureInDB, requesting_user: UserInDB) -> int:
        return await self.db.execute(query=DELETE_SIGNATURE_BY_ID_QUERY, values={"id": signature.id, "owner": requesting_user.id})



