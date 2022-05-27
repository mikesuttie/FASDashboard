from typing import List

from fastapi import APIRouter, Body, Depends
from starlette.status import HTTP_201_CREATED

from app.models.user import UserInDB
from app.models.patient import PatientCreate, PatientPublic
from app.db.repositories.patients import PatientsRepository
from app.api.dependencies.database import get_repository
from app.api.dependencies.patients import get_current_active_user, check_patient_creation_permissions, check_patient_modification_permissions, check_patients_reading_permissions, get_patient_by_id_from_path

router = APIRouter()

@router.get("/", name="patients:return-all-patients", status_code=HTTP_201_CREATED, 
    #dependencies=[Depends(check_patients_reading_permissions)]
    )
async def get_all_patients(
    patients_repo: PatientsRepository = Depends(get_repository(PatientsRepository)),
    current_user: UserInDB = Depends(get_current_active_user),    
) -> List[dict]:

    patients = await patients_repo.get_all_patients(requesting_user=current_user)    

    return patients


@router.post("/", response_model=PatientPublic, name="patients:create-patient", 
    status_code=HTTP_201_CREATED, dependencies=[Depends(check_patient_creation_permissions)])
async def create_new_patient(
    new_patient: PatientCreate = Body(..., embed=True),
    patients_repo: PatientsRepository = Depends(get_repository(PatientsRepository)),
    current_user: UserInDB = Depends(get_current_active_user),
) -> PatientPublic:
    created_patient = await patients_repo.create_patient(new_patient=new_patient, requesting_user=current_user)
    return created_patient
