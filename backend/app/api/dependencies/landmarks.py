from fastapi import HTTPException, Depends, Path, status

from app.models.user import UserInDB
from app.models.landmark import LandmarkInDB
from app.db.repositories.landmarks import LandmarksRepository
from app.api.dependencies.database import get_repository
from app.api.dependencies.auth import get_current_active_user


async def get_landmark_by_id_from_path(
    landmark_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    landmarks_repo: LandmarksRepository = Depends(get_repository(LandmarksRepository)),
) -> LandmarkInDB:

    landmark = await landmarks_repo.get_landmark_by_id(id=landmark_id, requesting_user=current_user)
    
    if not landmark:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND, detail="No landmark set found with that id.",
        )
        
    return landmark
    
    
def check_landmark_modification_permissions(
    current_user: UserInDB = Depends(get_current_active_user),
    landmark: LandmarkInDB = Depends(get_landmark_by_id_from_path),
) -> None:

    if landmark.owner != current_user.id:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN, 
            detail="Action forbidden. Users are only able to modify landmarks they own.",
        )

