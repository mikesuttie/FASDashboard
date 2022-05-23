from typing import List

from fastapi import APIRouter, Body, Depends, HTTPException, Path, UploadFile, File
from starlette.status import HTTP_201_CREATED, HTTP_404_NOT_FOUND

from app.models.user import UserCreate, UserUpdate, UserInDB, UserPublic
from app.models.landmark import LandmarkCreate, LandmarkPublic, LandmarkInDB
from app.db.repositories.landmarks import LandmarksRepository
from app.db.repositories.facescans import FacescansRepository
from app.api.dependencies.database import get_repository
from app.api.dependencies.auth import get_current_active_user
from app.api.dependencies.landmarks import get_landmark_by_id_from_path, check_landmark_modification_permissions

router = APIRouter()


@router.get("/", response_model=LandmarkPublic, name="landmarks:get-all-landmarks-owned-by-user")
async def get_all_landmarks_owned_by_user(
    current_user: UserInDB = Depends(get_current_active_user),
    landmarks_repo: LandmarksRepository = Depends(get_repository(LandmarksRepository)),
) -> LandmarkPublic:
    landmarks = await landmarks_repo.get_all_landmarks_owned_by_user(requesting_user=current_user)
    if not landmarks:
        raise HTTPException(status_code=HTTP_404_NOT_FOUND, detail="No landmarks found for current user.")
    return landmarks

@router.get("/landmarkRecords/{facescan_id}", response_model=List[int], name="landmarks:get-landmark-records-under-facescan-id")
async def get_landmarks_by_facescan_id(
    facescan_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    landmarks_repo: LandmarksRepository = Depends(get_repository(LandmarksRepository)),
) -> List[int]: 
    landmarksRecordIDs = await landmarks_repo.get_landmarks_under_facescan_id(facescan_id=facescan_id, requesting_user=current_user)
    if not landmarksRecordIDs:
        raise HTTPException(status_code=HTTP_404_NOT_FOUND, detail="No landmarks found under this facescan id.")
    return landmarksRecordIDs


@router.get("/{landmark_id}", response_model=LandmarkPublic, name="landmarks:get-landmarks-by-landmark-id")
async def get_landmarks_by_landmark_id(
    landmark_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    landmarks_repo: LandmarksRepository = Depends(get_repository(LandmarksRepository)),
) -> LandmarkPublic:
    landmarks = await landmarks_repo.get_landmarks_by_landmark_id(landmark_id=landmark_id, requesting_user=current_user)
    if not landmarks:
        raise HTTPException(status_code=HTTP_404_NOT_FOUND, detail="No landmarks found with that landmark id.")
    return landmarks


@router.get("/stringonly/{landmark_id}", response_model=dict, name="landmarks:get-landmarkstring-only-by-landmark-id")
async def get_landmarkstring_only_by_landmark_id(
    landmark_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    landmarks_repo: LandmarksRepository = Depends(get_repository(LandmarksRepository)),
) -> dict:
    landmarks = await landmarks_repo.get_landmarkstringonly_by_landmark_id(landmark_id=landmark_id, requesting_user=current_user)
    if not landmarks:
        raise HTTPException(status_code=HTTP_404_NOT_FOUND, detail="No landmarkstring found with that landmark id.")
    return landmarks


@router.post("/computeCNN20ptLandmarksFor/{facescan_id}", name="landmark:trigger-detection", status_code=HTTP_201_CREATED)
async def trigger_landmarkdetection_by_facescan_id(
    facescan_id: int = Path(..., ge=1, title="Facescan ID."),
    current_user: UserInDB = Depends(get_current_active_user),
    landmarks_repo: LandmarksRepository = Depends(get_repository(LandmarksRepository)),
    facescans_repo: FacescansRepository = Depends(get_repository(FacescansRepository)),    
) -> dict:

    facescanMesh = await facescans_repo.download_facescan_mesh(facescan_id=facescan_id, requesting_user=current_user)    
    if not facescanMesh:
        raise HTTPException(
            status_code=HTTP_404_NOT_FOUND, 
            detail="No facescan mesh under that facescan id exists.",
        )


    facescanTexture = await facescans_repo.download_facescan_texture(facescan_id=facescan_id, requesting_user=current_user)    
    if not facescanTexture:
        raise HTTPException(
            status_code=HTTP_404_NOT_FOUND, 
            detail="No facescan texture under that facescan id exists.",
        )
        
    detectedLandmarks = await landmarks_repo.get_detected_landmarks_on_facescan(texture=facescanTexture.texture, mesh=facescanMesh.mesh) 
    
    if not detectedLandmarks:
        raise HTTPException(
            status_code=HTTP_404_NOT_FOUND, 
            detail="Landmark detection failed.",
        )
        
    insertedLandmarkID = await landmarks_repo.insertNewLandmarks(facescan_id=facescan_id, landmarks=detectedLandmarks, landmarkset="CNN20points", requesting_user=current_user) 
            
    return insertedLandmarkID
    
       
@router.delete(
    "/{landmark_id}/",
    response_model=int,
    name="landmark:delete-landmark-by-id",
    dependencies=[Depends(check_landmark_modification_permissions)],
)
async def delete_landmark_by_id(
    landmark: LandmarkInDB = Depends(get_landmark_by_id_from_path),
    current_user: UserInDB = Depends(get_current_active_user),    
    landmarks_repo: LandmarksRepository = Depends(get_repository(LandmarksRepository)),
) -> int:
    return await landmarks_repo.delete_landmark_by_id(landmark=landmark)
    
