from typing import List

from fastapi import APIRouter, Body, Depends, HTTPException, Path, UploadFile, File
from starlette.status import HTTP_201_CREATED, HTTP_404_NOT_FOUND

from app.models.user import UserCreate, UserUpdate, UserInDB, UserPublic
from app.models.meanClassification import MeanClassificationCreate, MeanClassificationPublic, MeanClassificationInDB
from app.db.repositories.meanClassifications import MeanClassificationRepository
from app.api.dependencies.database import get_repository
from app.api.dependencies.auth import get_current_active_user
from app.api.dependencies.meanClassifications import get_meanClassification_by_id_from_path, check_meanClassification_modification_permissions

router = APIRouter()


@router.get("/{landmark_id}", response_model=List[MeanClassificationPublic], name="meanClassifications:get-meanClassifications-by-landmark-id")
async def get_meanClassifications_by_landmark_id(
    landmark_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    meanClassification_repo: MeanClassificationRepository = Depends(get_repository(MeanClassificationRepository)),
) -> List[MeanClassificationPublic]:
   
    # Return mean classification results belonging to a landmark ID.
    meanClassificationRecords = await meanClassification_repo.get_meanClassificationRecord(landmark_id = landmark_id, requesting_user = current_user)

    return meanClassificationRecords


@router.post("/{landmark_id}", name="meanClassifications:create-new-record", status_code=HTTP_201_CREATED)
async def create_meanClassificationsRecord(
    landmark_id: int = Path(..., ge=1, title="Landmark ID."),
    new_meanClassificationResult: MeanClassificationCreate = Body(..., embed=True),    
    current_user: UserInDB = Depends(get_current_active_user),
    meanClassification_repo: MeanClassificationRepository = Depends(get_repository(MeanClassificationRepository)),
) -> MeanClassificationPublic:
            
    created_meanClassificationRecord = await meanClassifciations_repo.create_meanClassificationResultRecord(new_record=new_meanClassificationResult, signature_id = signature_id, requesting_user=current_user)
    
    return created_meanClassificationRecord
    

# Invoke computation of meanClassification record. Returns meanClassificationRecord ID.
@router.post("/compute/{landmark_id}", response_model=MeanClassificationInDB, name="meanClassifications:trigger-computation", status_code=HTTP_201_CREATED)
async def trigger_MeanClassificationscomputation_by_landmark_id(
    meanClassificationmodel_name: str = "CAUC16pt",
    facialregion_code: str = "FACE",     
    landmark_id: int = Path(..., ge=1, title="Landmark ID."),
    current_user: UserInDB = Depends(get_current_active_user),
    meanClassification_repo: MeanClassificationRepository = Depends(get_repository(MeanClassificationRepository))
):
            
    insertedMeanClassificationRecordID = await meanClassification_repo.compute_meanClassication(landmark_id = landmark_id, 
                                                                                       meanClassificationmodel_name = meanClassificationmodel_name, 
                                                                                       facialregion_code = facialregion_code, 
                                                                                       requesting_user = current_user)
    return insertedMeanClassificationRecordID
    

@router.delete(
    "/{id}/", #meanClassificationRecord_id
    response_model=int  ,
    name="meanClassificationResult:delete-by-id",
    dependencies=[Depends(check_meanClassification_modification_permissions)],
)
async def delete_meanClassificationRecord_by__landmark_id(
    meanClassificationRecord: MeanClassificationInDB = Depends(get_meanClassification_by_id_from_path),
    meanClassifications_repo: MeanClassificationRepository = Depends(get_repository(MeanClassificationRepository)),
    current_user: UserInDB = Depends(get_current_active_user),    
) -> int:
    return await meanClassifications_repo.delete_meanclassification_by_id(meanclassificationRecord=meanClassificationRecord, requesting_user=current_user)
    
