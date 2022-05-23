from typing import List

from fastapi import APIRouter, Body, Depends, HTTPException, Path, UploadFile, File
from starlette.status import HTTP_201_CREATED

from app.models.user import UserCreate, UserUpdate, UserInDB, UserPublic
from app.models.patientnote import PatientnoteCreate, PatientnotePublic, PatientnoteInDB
from app.db.repositories.patientnotes import PatientnotesRepository
from app.api.dependencies.database import get_repository
from app.api.dependencies.auth import get_current_active_user
from app.api.dependencies.patientnotes import get_patientnote_by_id_from_path, check_patientnote_modification_permissions

router = APIRouter()

@router.get("/patientnotes", response_model=List[PatientnotePublic], name="patientnotes:list-all-user-owned-patientnotes")
async def list_all_user_owned_patientnotess(
    current_user: UserInDB = Depends(get_current_active_user),
    patientnotes_repo: PatientnotesRepository = Depends(get_repository(PatientnotesRepository)),
) -> List[PatientnotePublic]:
    return await patientnotes_repo.get_all_patientnotes_owned_by_user(requesting_user=current_user)

@router.get("/{patient_id}/patientnotes", response_model=List[PatientnotePublic], name="patientnotes:list-patientnotes-by-patient-id")
async def list_all_user_owned_patientnotess_by_patient_id(
    patient_id: int = Path(..., ge=1, title="Patient ID."),
    current_user: UserInDB = Depends(get_current_active_user),
    patientnotes_repo: PatientnotesRepository = Depends(get_repository(PatientnotesRepository)),
) -> List[PatientnotePublic]:
    return await patientnotes_repo.get_all_patientnotes_for_patient(requesting_user=current_user,patient_id=patient_id)

""" # Note: No corresponding repository API (and SQL query) yet. Is it really useful?
@router.get("/{patient_id}/patientnotes/{patientnote_id}/", response_model=PatientnotePublic, name="patientnotes:get-patientnote-by-id")
async def get_patientnote_by_id(
    patientnote_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    patientnotes_repo: PatientnotesRepository = Depends(get_repository(PatientnotesRepository)),
) -> PatientnotePublic:
    patientnote = await patientnotes_repo.get_patientnote_by_id(id=patientnotes_id, requesting_user=current_user)
    if not patientnote:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="No patient note found with that id.")
    return patientnote
"""


@router.post("/{patient_id}/patientnotes", response_model=PatientnotePublic, name="patientnotes:create-patientnote", status_code=HTTP_201_CREATED)
async def create_new_patientnote(
    patient_id: int = Path(..., ge=1, title="Patient ID."),
    new_patientnote: PatientnoteCreate = Body(..., embed=True),
    current_user: UserInDB = Depends(get_current_active_user),
    patientnotes_repo: PatientnotesRepository = Depends(get_repository(PatientnotesRepository))
) -> PatientnotePublic:
    created_patientnote = await patientnotes_repo.create_patientnote(new_patientnote=new_patientnote, requesting_user=current_user)
    return created_patientnote
    
      
@router.put("/{patient_id}/patientnotes/{patientnote_id}", response_model=PatientnotePublic, name="patientnotes:update-patientnotes", status_code=HTTP_201_CREATED)
async def update_patientnote_by_id(
    patient_id: int = Path(..., ge=1, title="Patient ID."),
    patietentnote_id: int = Path(..., ge=1, title="Patientnote ID."),
    patientnote: str = "",
    current_user: UserInDB = Depends(get_current_active_user),
    patientnotes_repo: PatientnotesRepository = Depends(get_repository(PatientnotesRepository))
) -> PatientnotePublic:

    patientnoteRecord = await patientnotes_repo.update_patientnote_by_id(patientnote_id=patientnote_id, patientnote=patientnote, requesting_user=current_user)
    
    if not patientnoteRecord:
        raise HTTPException(
            status_code=HTTP_404_NOT_FOUND, 
            detail="No patient note with that id exists.",
        )

    return patientnoteRecord      
    
       
@router.delete(
    "/patientnotes/{patientnote_id}/",
    response_model=int,
    name="patientnotes:delete-patientnote-by-id",
    dependencies=[Depends(check_patientnote_modification_permissions)],
)
async def delete_patientnote_by_id(
    patientnote: PatientnoteInDB = Depends(get_patientnote_by_id_from_path),
    patientnotes_repo: PatientnotesRepository = Depends(get_repository(PatientnotesRepository)),
) -> int:
    return await patientnotes_repo.delete_patientnote_by_id(patientnote=patientnote) 
    
