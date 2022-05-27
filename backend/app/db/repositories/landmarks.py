from app.db.repositories.base import BaseRepository
from app.models.landmark import LandmarkInDB #, LandmarkUpdate, LandmarkCreate
from app.models.user import UserInDB

from fastapi import HTTPException, status
from typing import List

import asyncpg.exceptions
import httpx
import json
import math

# Round float to n significant figures
def round_to_n(x, n):
    if not x: return 0
    power = -int(math.floor(math.log10(abs(x)))) + (n - 1)
    factor = (10 ** power)
    return round(x * factor) / factor

# Round the floats to reasonable precision to save mem (intended for use in json output)
def round_floats(o):
    if isinstance(o, float):
        return round_to_n(o,5) 
    if isinstance(o, dict):
        return {k: round_floats(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [round_floats(x) for x in o]
    return o
    

GET_ALL_LANDMARKS_OWNED_BY_USER_QUERY = """
    SELECT id, facescan_id, landmarksetcode, algorithm, computationstatus, landmarks, owner, created_at, updated_at
    FROM landmarks
    WHERE owner = :owner;
"""

GET_LANDMARKRECORDSS_BY_FACESCAN_ID_QUERY = """
    SELECT id, facescan_id, landmarksetcode, algorithm, computationstatus, landmarks, owner, created_at, updated_at
    FROM landmarks
    WHERE owner = :owner AND facescan_id = :facescan_id;
"""

GET_LANDMARKS_BY_LANDMARK_ID_QUERY = """
    SELECT id, facescan_id, landmarksetcode, algorithm, computationstatus, landmarks, owner, created_at, updated_at
    FROM landmarks
    WHERE id = :landmark_id AND owner = :owner;
"""

GET_LANDMARKSTRING_ONLY_BY_LANDMARK_ID_QUERY = """
    SELECT landmarks
    FROM landmarks
    WHERE id = :landmark_id AND owner = :owner;
"""

INSERT_NEW_LANDMARKS_RECORD = """
    INSERT INTO landmarks (facescan_id, landmarksetcode, algorithm, computationstatus, landmarks, owner)
    VALUES (:facescan_id, :landmarksetcode, :algorithm, :computationstatus, :landmarks, :owner)
    RETURNING id;
"""

DELETE_LANDMARK_BY_ID_QUERY = """
    DELETE FROM landmarks
    WHERE id = :id AND owner = :owner
    RETURNING id;
"""


class LandmarksRepository(BaseRepository):

    async def get_all_landmarks_owned_by_user(self, requesting_user: UserInDB) -> List[LandmarkInDB]:
        """
            Returns list of landmark sets related to facescans owned by requesting user. 
            This could include, e.g., intial user annotations and computed landmarks.
        """               
        alllandmarks = await self.db.fetch_all(query=GET_ALL_LANDMARKS_OWNED_BY_USER_QUERY, values={"owner": requesting_user.id})
        return [LandmarkInDB(**landmark) for landmark in alllandmarks]


    async def get_landmarks_under_facescan_id(self, *, facescan_id: int, requesting_user: UserInDB)->List[int]: #List[LandmarkInDB]:
        """
            Returns list of landmark record IDs corresponding to facescan_id, but only if owned by requesting user. 
            This could include, e.g., intial user annotations and computed landmarks.
        """               
        landmarksRecords = await self.db.fetch_all(query=GET_LANDMARKRECORDSS_BY_FACESCAN_ID_QUERY, values={"facescan_id": facescan_id, "owner": requesting_user.id})

        landmarkIDlist = []
        for rec in landmarksRecords:
            landmarkIDlist.append(tuple(rec.values())[0])  #TODO fix this!

        return landmarkIDlist # [LandmarkInDB(**landmarkRecord) for landmarkRecord in landmarksRecords] 


    async def get_landmarks_by_landmark_id(self, *, landmark_id: int, requesting_user: UserInDB) -> LandmarkInDB:
        """
            Returns landmarks corresponding to landmark_id, but only if owned by requesting user. 
            This could include, e.g., intial user annotations and computed landmarks.
        """               
        landmarksRec = await self.db.fetch_one(query=GET_LANDMARKS_BY_LANDMARK_ID_QUERY, values={"landmark_id": landmark_id, "owner": requesting_user.id})
        # print('LANDMARKS..........................')
        # landmarks.landmarks = json.loads(landmarks.landmarks)
        # for rec in landmarksRec:
        #    print(tuple(rec.values()))  #TODO fix this!
            
        return LandmarkInDB(**landmarksRec)  #TODO The solution to fixing this mess includes getting the json data from the Postgres DB validated by Pydantic.


    async def get_landmarkstringonly_by_landmark_id(self, *, landmark_id: int, requesting_user: UserInDB) -> str:
        """
            Returns landmarks corresponding to landmark_id, but only if owned by requesting user. 
            Landmark field of landmark table only, no other metadata
        """               
        landmarksRec = await self.db.fetch_one(query=GET_LANDMARKSTRING_ONLY_BY_LANDMARK_ID_QUERY, values={"landmark_id": landmark_id, "owner": requesting_user.id})

        """
        for i,rec in enumerate(landmarksRec):
            print(i)
            print(rec.values())
            if (rec.values()=='landmarks'):
                print(tuple(rec.values())) """ 
        if landmarksRec is None:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND, detail="No landmark data found under this landmark ID.",
            )                                       
        try:
            landmarkResults = landmarksRec[0] #.values()
        except IndexError:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND, detail="No landmark data found under this landmark ID (result set empty).",
            )                
        return round_floats(json.loads( (''.join(tuple(landmarkResults))) ))
        
                       
    async def delete_landmark_by_id(self, *, landmark: LandmarkInDB) -> int:
        return await self.db.execute(query=DELETE_LANDMARK_BY_ID_QUERY, values={"id": landmark.id})
    

    async def get_detected_landmarks_on_facescan(self, *, texture, mesh) -> dict:
        """ Forwards to landmarkingserver streams of texture and mesh from database. """
        headGeometryData = [('faceFiles',mesh), ('faceFiles',texture)]
        try:
            r = httpx.post("http://landmarkingserver:7998/detectLandmarks/", files=headGeometryData, timeout=100.0)                
        except httpx.ConnectError:
            print("Connection refused to landmarkingserver!")
        """
        try:
            s = httpx.get("http://facescreenprocessor:34568/", timeout=1.0)                
            print( s.json())
        except httpx.ConnectError:
            print("Connection refused to facescreenprocessor!")
        """

        return r.json()
        

    async def insertNewLandmarks(self, *, facescan_id, landmarks, landmarkset, requesting_user: UserInDB) -> int:       
        newLandmarksID = await self.db.fetch_one(query=INSERT_NEW_LANDMARKS_RECORD, 
            values={"facescan_id": facescan_id, 
                    "landmarksetcode": landmarkset, 
                    "algorithm": "prototypeDLIB+CNN", 
                    "computationstatus": "ok", 
                    "landmarks": json.dumps(landmarks),
                    "owner": requesting_user.id})
        return newLandmarksID


        
