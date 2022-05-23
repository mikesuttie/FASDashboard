from typing import Dict

from fastapi import APIRouter, Body, Depends, HTTPException, Path, UploadFile, File
from starlette.status import HTTP_201_CREATED, HTTP_404_NOT_FOUND

from app.models.user import UserCreate, UserUpdate, UserInDB, UserPublic
from app.models.landmark import LandmarkPublic, LandmarkInDB
from app.db.repositories.landmarks import LandmarksRepository
from app.api.dependencies.database import get_repository
from app.api.dependencies.auth import get_current_active_user
from app.api.dependencies.landmarks import get_landmark_by_id_from_path, check_landmark_modification_permissions

from math import sqrt

router = APIRouter()


@router.get("/{landmark_id}", response_model=Dict, name="PFL:get-PFL-by-landmark-id")
async def get_PFL_by_landmark_id(
    landmark_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    landmarks_repo: LandmarksRepository = Depends(get_repository(LandmarksRepository)),
) -> Dict:

    landmarksRecord = await landmarks_repo.get_landmarkstringonly_by_landmark_id(landmark_id=landmark_id, requesting_user=current_user)
    if not landmarksRecord:
        raise HTTPException(status_code=HTTP_404_NOT_FOUND, detail="No landmarks found with that landmark id.")

    #  raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="No PFL analysis available.")

    pflAnalysis = {}
    
    def distanceEuclid3D(p1, p2): 
      return sqrt((p1[0]-p2[0])**2+(p1[1]-p2[1])**2+(p1[2]-p2[2])**2)
    
    try:
        landmarks = landmarksRecord['consensus']
                
        landmark_right_en = landmarks['right_en']
        landmark_right_ex = landmarks['right_ex']
        landmark_left_en = landmarks['left_en']
        landmark_left_ex = landmarks['right_ex']
            
        pflAnalysis['PFLleftEye']  = round(distanceEuclid3D(landmarks['left_en'], landmarks['left_ex']),3)
        pflAnalysis['PFLrightEye'] = round(distanceEuclid3D(landmarks['right_en'], landmarks['right_ex']),3)
        pflAnalysis['PFLraw'] = (pflAnalysis['PFLleftEye'] + pflAnalysis['PFLrightEye']) / 2.0
    except Exception as e:         
        print("PFL analysis failed. Error: " + e)
        raise HTTPException(status_code=HTTP_404_NOT_FOUND, detail="Not all landmarks required for PFL analysis are available.")
        
    return pflAnalysis
    
    
"""
# This function is not required, as long as PFL computation can be done on-the-fly. In this case the GET EP is sufficient.
@router.post("/{landmark_id}", name="PFL:trigger-computation", status_code=HTTP_201_CREATED)
async def trigger_PFLcomputation_by_landmark_id(
    landmark_id: int = Path(..., ge=1, title="Facescan ID."),
    current_user: UserInDB = Depends(get_current_active_user)
):
        
    #TODO
    raise HTTPException(
        status_code=HTTP_404_NOT_FOUND, 
        detail="PFL computation currently not connected to compute server.",
    )
    
"""
    
