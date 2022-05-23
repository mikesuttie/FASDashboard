from fastapi import HTTPException, Depends, Path, status

from app.models.user import UserInDB
from app.models.patient import PatientInDB
from app.db.repositories.patients import PatientsRepository
from app.api.dependencies.database import get_repository
from app.api.dependencies.auth import get_current_active_user


async def get_patient_by_id_from_path(
    patient_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    patients_repo: PatientsRepository = Depends(get_repository(PatientsRepository)),
) -> PatientInDB:

    patient = await patients_repo.get_patient_by_id(id=patient_id, requesting_user=current_user)
    
    if not patient:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND, detail="No patient found with that id.",
        )
        
    return patient
    
    
def check_patient_modification_permissions(
    current_user: UserInDB = Depends(get_current_active_user),
    patient: PatientInDB = Depends(get_patient_by_id_from_path),
) -> None:

    if patient.owner != current_user.id:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN, 
            detail="Action forbidden. Users are only able to modify patient records they own.",
        )

def check_patients_reading_permissions(
    current_user: UserInDB = Depends(get_current_active_user),
    patient: PatientInDB = Depends(get_patient_by_id_from_path),    
) -> None:

    if patient.owner != current_user.id:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN, 
            detail="Action forbidden. Users are only able to read patient records they own.",
        )

def check_patient_creation_permissions(
    current_user: UserInDB = Depends(get_current_active_user),
) -> None:

    pass

