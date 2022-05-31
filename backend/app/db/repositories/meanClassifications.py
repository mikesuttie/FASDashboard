from fastapi import HTTPException, status
from typing import List
from asyncpg.exceptions import ForeignKeyViolationError

from app.db.repositories.base import BaseRepository
from app.models.meanClassification import MeanClassificationCreate, MeanClassificationInDB, MeanClassificationPublic
from app.models.user import UserInDB

import httpx
import json

CREATE_MEANCLASSIFICATION_RECORDS_BY_LANDMARK_ID_QUERY = """
    INSERT INTO meanclassificationresults (landmark_id, modelname, facialregion, meanvalue, stderror, pos_class, neg_class, owner)
    VALUES (:landmark_id, :modelname, :facialregion, :meanvalue, :stderror, :pos_class, :neg_class, :owner)
    RETURNING id, landmark_id, modelname, facialregion, meanvalue, stderror, pos_class, neg_class, owner, created_at, updated_at;
"""

GET_MEANCLASSIFICATION_RECORD_BY_ID_QUERY = """
    SELECT id, landmark_id, modelname, facialregion, meanvalue, stderror, pos_class, neg_class, owner, created_at, updated_at
    FROM meanclassificationresults
    WHERE owner = :owner AND id = :id;
"""

GET_MEANCLASSIFICATION_RECORD_BY_LANDMARK_ID_QUERY = """
    SELECT id, landmark_id, modelname, facialregion, meanvalue, stderror, pos_class, neg_class, owner, created_at, updated_at
    FROM meanclassificationresults
    WHERE owner = :owner AND landmark_id = :landmark_id;
"""

DELETE_MEANCLASSIFICATIONRECORD_BY_ID_QUERY = """
    DELETE FROM meanclassificationresults
    WHERE owner = :owner AND id = :id
    RETURNING id;
"""


# Queries to retrieve data for server-internal processing : face mesh and landmarks
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

GET_FACIALREGIONNAME_FROM_FACIALREGIONCODE = """
    SELECT facialregionname
    FROM facialregionnames
    WHERE code = :code;
"""


class MeanClassificationRepository(BaseRepository):
    """
    """

    async def create_meanclassificationresult_record(self, *, new_meanclassificationresult: MeanClassificationCreate, requesting_user: UserInDB) -> MeanClassificationInDB:
        query_values = new_meanClassificationResult.dict()
        try:
            meanClassificationRecord = await self.db.fetch_one(query=CREATE_MEANCLASSIFICATION_RECORDS_BY_LANDMARK_ID_QUERY, values={**query_values, 
                #"landmark_id": new_meanClassificationResult.landmark_id,
                #"modelname": new_meanclassificationresult.modelname,  
                #"facialregion": new_meanclassificationresult.facialregion,  
                #"meanvalue": new_meanClassificationResult.meanvalue, 
                #"stderror": new_meanClassificationResult.stderror, 
                #"pos_class": new_meanclassificationresult.pos_class,
                #"neg_class": new_meanclassificationresult.neg_class,
                "owner": requesting_user.id})
        except (ForeignKeyViolationError):
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail="Cannot insert meanclassfication records, as landmark ID does not exist: Foreign Key Violation Error.",
                headers={"WWW-Authenticate": "Bearer"},
            )                         
        return MeanClassificationInDB(**meanClassificationRecord)


    async def compute_meanClassication(self, *, landmark_id: int, meanClassificationmodel_name: str, facialregion_code: str, requesting_user: UserInDB) -> MeanClassificationInDB:
        
        facialRegionNameInDBRecord = await self.db.fetch_one(query=GET_FACIALREGIONNAME_FROM_FACIALREGIONCODE, values={"code": facialregion_code})    
        if facialRegionNameInDBRecord is None:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail="Facial region code does not exist or is not supported.",
                headers={"WWW-Authenticate": "Bearer"},        
            )
        facialRegionNameInDB = facialRegionNameInDBRecord[0]
        print("Processing facial region: ", facialRegionNameInDB)
        with httpx.Client() as client:    
            try:
                faceMeshInDB     = await self.db.fetch_one(query=GET_FACEMESH_BY_LANDMARK_ID,                  values={"landmark_id": landmark_id, "owner": requesting_user.id})         
                landmarksInDB    = await self.db.fetch_one(query=GET_LANDMARKSTRING_ONLY_BY_LANDMARK_ID_QUERY, values={"landmark_id": landmark_id, "owner": requesting_user.id})         
            except (ForeignKeyViolationError) as err:
                raise HTTPException(
                    status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                    detail="Landmark principal_componentsID OR Facial Region: Foreign Key Violation Error: " + err.message,
                    headers={"WWW-Authenticate": "Bearer"},
                )          
            try:            
                # print("Landmarks: ", landmarksInDB[0])            
                landmarksInDB = json.loads(landmarksInDB[0])["consensus"]
            except:                
                raise HTTPException(
                    status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                    detail="Consensus landmarks not available in database! Cannot register face mesh.",
                    headers={"WWW-Authenticate": "Bearer"},
                )                            
          
            try:
                processingserver_response = client.get("http://facescreenprocessor:34568/faceScreen/processor/processingToken/", timeout=1.0) 
            except httpx.ConnectError:
                print("In meanClassfication computation: Getting processing token: Connection refused to processingserver!")
                raise HTTPException(
                    status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                    detail="Processing server refused to issue processing token!",
                    headers={"WWW-Authenticate": "Bearer"},
                )
            
            processingToken = processingserver_response.text

            try:
                #TODO? select ethnicity through API or query from DB. Currently encoded in classification model name.
                processingserver_response = client.post("http:DELETE_MEANCLASSIFICATIONRECORD_BY_ID_QUERY//facescreenprocessor:34568/faceScreen/processor/landmarks", timeout=1.0, params = {'processingToken': processingToken, 'ethnicityCode': 'CAUC'}, json=landmarksInDB)                  
            except (httpx.ConnectError,httpx.ReadTimeout) as err:
                print("In meanClassfication computation: Transferring landmarks: Connection refused to processingserver!", type(err), err.message)
                raise HTTPException(
                    status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                    detail="Processing server refused to accept landmarks data: " + err.message,
                    headers={"WWW-Authenticate": "Bearer"},
                )

            #print("UPLOADED LANDMARKS TO PROCESSNIG SERVER")

            try:
                processingserver_response = client.put("http://facescreenprocessor:34568/faceScreen/processor/objFile?processingToken=" + processingToken , timeout=3.0, data=faceMeshInDB[0])
            except httpx.ConnectError:
                print("In meanClassfication computation: Uploading mesh: Connection refused to processingserver!")      
                raise HTTPException(
                    status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                    detail="Processing server refused to accept face mesh data!",
                    headers={"WWW-Authenticate": "Bearer"},
                )
            except httpx.ReadTimeout:
                print("In meanClassfication computation: Processingserver exceeded reading time. Try again!")
                raise HTTPException(
                    status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                    detail="Timeout error when transferring face mesh data to processing server!",
                    headers={"WWW-Authenticate": "Bearer"},
                )
            #print("UPLOADED FACE MESH TO PROCESSINGSERVER")

            try:
                processingserver_response = client.get("http://facescreenprocessor:34568/faceScreen/processor/computeClassification", timeout=10.0, params = {'processingToken': processingToken, 'facialRegion': facialRegionNameInDB}) #TODO: Return error if no classification model corresponding to this facicalREgion_code exists
                #TODO return error if no model for ethnicity (uploaded jointly with landmarks) exists
            except httpx.ConnectError:
                print("Computing meanClassfication: Connection refused to processingserver!")
            except httpx.ReadTimeout:
                print("Computing meanClassfication: Processingserver exceeded reading time. Try again!")
                raise HTTPException(
                    status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                    detail="Request to processing server timed out while computing heatmap!",
                    headers={"WWW-Authenticate": "Bearer"},
                )
            #print("COMPUTED MEANCLASSIFICATION")
            
            try:
                processingserver_response = client.get("http://facescreenprocessor:3456MeanClassificationInDB8/faceScreen/processor/classifications", params = {'processingToken': processingToken}, timeout=1.0)
            except httpx.ConnectError:
                print("Getting meanClassfication: Connection refused to processingserver!")
            except httpx.ReadTimeout:
                print("Computing meanClassfication: Processingserver exceeded reading time. Try again!")
                raise HTTPException(
                    status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                    detail="Request to processing server timed out while loading meanClassfication result!",
                    headers={"WWW-Authenticate": "Bearer"},
                )            
                
            #print("meanClassfication data: " , processingserver_response.content, facialRegionNameInDB)    
            try:        
                meanClassificationResult = json.loads(processingserver_response.content)[facialRegionNameInDB]
            except:
                raise HTTPException(
                    status_code=status.HTTP_404_NOT_FOUND,
                    detail="Processing server did not return classification results for requested facial region. Likely, no corresponding model is available.",
                    headers={"WWW-Authenticate": "Bearer"},        
                )                
            try:
                meanClassificationRecordToStoreInDB = await self.db.fetch_one(query=CREATE_MEANCLASSIFICATION_RECORDS_BY_LANDMARK_ID_QUERY, \
                    values={"landmark_id": landmark_id, 
                            "modelname": 'UNSPECIFIED', # TODO enforce foreign key constraint and use parameter: meanClassificationmodel_name,  
                            "facialregion": facialregion_code,  
                            "meanvalue": meanClassificationResult['mean'], 
                            "stderror": meanClassificationResult['stdError'], 
                            "pos_class": '',
                            "neg_class": '',
                            "owner": requesting_user.id})           
            except (ForeignKeyViolationError) as err:
                raise HTTPException(
                    status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                    detail="Landmark ID Foreign Key Violation Error when inserting new meanClassification record into DB: " + err.message,
                    headers={"WWW-Authenticate": "Bearer"},
                )          
            
            return MeanClassificationInDB(**meanClassificationRecordToStoreInDB)

    
    # Get all records from DB associated to a landmark record
    async def get_meanClassificationRecord(self, *, landmark_id: int, requesting_user: UserInDB) -> List[MeanClassificationPublic]:
        query_values = {}
        query_values['landmark_id'] = landmark_id
        query_values['owner'] = requesting_user.id
        meanClassificationRecords = await self.db.fetch_all(query=GET_MEANCLASSIFICATION_RECORD_BY_LANDMARK_ID_QUERY, values=query_values)
        return [MeanClassificationPublic(**meanClassificationRecord) for meanClassificationRecord in meanClassificationRecords]
    
    
    # Get a single record from DB
    async def meanClassification_by_id(self, *, id: int, requesting_user: UserInDB) -> MeanClassificationPublic:
        meanClassificationRecord = await self.db.fetch_one(query=GET_MEANCLASSIFICATION_RECORD_BY_ID_QUERY, values={"id": id, "owner": requesting_user.id})
        if meanClassificationRecord is None:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail="Mean classification record does not exist!",
                headers={"WWW-Authenticate": "Bearer"},        
            )        
        return MeanClassificationPublic(**meanClassificationRecord)
    
    
    async def delete_meanclassification_by_id(self, *, meanclassificationRecord: MeanClassificationInDB, requesting_user: UserInDB) -> int:
        return await self.db.execute(query=DELETE_MEANCLASSIFICATIONRECORD_BY_ID_QUERY, values={"id": meanclassificationRecord.id, "owner": requesting_user.id})

