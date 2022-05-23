from fastapi import HTTPException, Depends, Path, status

from app.models.user import UserInDB
from app.models.facescan import FacescanInDB
from app.db.repositories.facescans import FacescansRepository
from app.api.dependencies.database import get_repository
from app.api.dependencies.auth import get_current_active_user


async def get_facescan_by_id_from_path(
    facescan_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    facescans_repo: FacescansRepository = Depends(get_repository(FacescansRepository)),
) -> FacescanInDB:

    facescan = await facescans_repo.get_facescan_by_id(id=facescan_id, requesting_user=current_user)
    
    if not facescan:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND, detail="No facescan found with that id.",
        )
        
    return facescan
    
    
def check_facescan_modification_permissions(
    current_user: UserInDB = Depends(get_current_active_user),
    facescan: FacescanInDB = Depends(get_facescan_by_id_from_path),
) -> None:

    if facescan.owner != current_user.id:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN, 
            detail="Action forbidden. Users are only able to modify facescans they own.",
        )

