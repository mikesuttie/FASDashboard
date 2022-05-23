from fastapi import HTTPException, Depends, Path, status

from app.models.user import UserInDB
from app.models.meanClassification import MeanClassificationInDB
from app.db.repositories.meanClassifications import MeanClassificationRepository
from app.api.dependencies.database import get_repository
from app.api.dependencies.auth import get_current_active_user


async def get_meanClassification_by_signature_id_from_path(
    signature_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    meanClassifications_repo: MeanClassificationRepository = Depends(get_repository(MeanClassificationRepository)),
) -> MeanClassificationInDB:

    meanClassification = await meanClassifications_repo.meanClassification_by_id(signature_id=signature_id, requesting_user=current_user)
    
    if not meanClassification:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND, detail="No signature found with that id or no mean classification available under that signature ID.",
        )
        
    return signature
    
    
def check_meanClassification_modification_permissions(
    current_user: UserInDB = Depends(get_current_active_user),
    meanClassification: MeanClassificationInDB = Depends(get_meanClassification_by_signature_id_from_path),
) -> None:

    if meanClassification.owner != current_user.id:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN, 
            detail="Action forbidden. Users are only able to modify signatures they own.",
        )

