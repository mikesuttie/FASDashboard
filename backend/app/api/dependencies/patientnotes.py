from fastapi import HTTPException, Depends, Path, status

from app.models.user import UserInDB
from app.models.patientnote import PatientnoteInDB
from app.db.repositories.patientnotes import PatientnotesRepository
from app.api.dependencies.database import get_repository
from app.api.dependencies.auth import get_current_active_user


async def get_patientnote_by_id_from_path(
    patientnote_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    patientnotes_repo: PatientnotesRepository = Depends(get_repository(PatientnotesRepository)),
) -> PatientnoteInDB:

    patientnote = await patientnotes_repo.get_patientnote_by_id(id=patientnote_id, requesting_user=current_user)
    
    if not patientnote:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND, detail="No patient note found with that id.",
        )
        
    return patientnote
    
    
def check_patientnote_modification_permissions(
    current_user: UserInDB = Depends(get_current_active_user),
    patientnote: PatientnoteInDB = Depends(get_patientnote_by_id_from_path),
) -> None:

    if patientnote.owner != current_user.id:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN, 
            detail="Action forbidden. Users are only able to modify patient notes they own.",
        )

