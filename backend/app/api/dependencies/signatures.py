from fastapi import HTTPException, Depends, Path, status

from app.models.user import UserInDB
from app.models.signature import SignatureInDB
from app.db.repositories.signatures import SignatureRepository
from app.api.dependencies.database import get_repository
from app.api.dependencies.auth import get_current_active_user


async def get_signature_by_id_from_path(
    signature_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    signatures_repo: SignatureRepository = Depends(get_repository(SignatureRepository)),
) -> SignatureInDB:

    signature = await signatures_repo.get_signature_by_id(id=signature_id, requesting_user=current_user)
    
    if not signature:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND, detail="No signature found with that id.",
        )
        
    return signature
    
    
def check_signature_modification_permissions(
    current_user: UserInDB = Depends(get_current_active_user),
    signature: SignatureInDB = Depends(get_signature_by_id_from_path),
) -> None:

    if signature.owner != current_user.id:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN, 
            detail="Action forbidden. Users are only able to modify signatures they own.",
        )

