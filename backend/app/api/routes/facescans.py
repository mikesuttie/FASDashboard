from typing import List

from fastapi import APIRouter, Body, Depends, HTTPException, Path, UploadFile, File, Response
from starlette.status import HTTP_201_CREATED, HTTP_404_NOT_FOUND

from app.models.user import UserCreate, UserUpdate, UserInDB, UserPublic
from app.models.facescan import FacescanCreate, FacescanPublic, FacescanInDB, FacescanMeshInDB, FacescanTextureInDB
from app.db.repositories.facescans import FacescansRepository
from app.api.dependencies.database import get_repository
from app.api.dependencies.auth import get_current_active_user
from app.api.dependencies.facescans import get_facescan_by_id_from_path, check_facescan_modification_permissions

router = APIRouter()


@router.get("/fromPatient/{patient_id}", name="patients:return-all-facescans", status_code=HTTP_201_CREATED)
async def get_all_facescans_for_patient(patient_id: int, 
    current_user: UserInDB = Depends(get_current_active_user),
    facescans_repo: FacescansRepository = Depends(get_repository(FacescansRepository)),) -> List[dict]:
    
    facescans = await facescans_repo.get_all_facescans_for_patient(patient_id=patient_id, requesting_user=current_user)
    return facescans


@router.get("/", response_model=List[FacescanPublic], name="facescans:list-all-user-owned-facescans")
async def list_all_user_owned_facescans(
    current_user: UserInDB = Depends(get_current_active_user),
    facescans_repo: FacescansRepository = Depends(get_repository(FacescansRepository)),
) -> List[FacescanPublic]:
    return await facescans_repo.list_all_facescans_owned_by_user(requesting_user=current_user)


@router.get("/{facescan_id}", response_model=FacescanPublic, name="facescans:get-facescan-by-id")
async def get_facescan_by_id(
    facescan_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    facescans_repo: FacescansRepository = Depends(get_repository(FacescansRepository)),
) -> FacescanPublic:
    facescan = await facescans_repo.get_facescan_by_id(id=facescan_id, requesting_user=current_user)
    if not facescan:
        raise HTTPException(status_code=HTTP_404_NOT_FOUND, detail="No facescan found with that id.")
    return facescan


@router.get("/patientAge/{facescan_id}", response_model=float, name="facescans:get-patient-age-of-facescan-by-id")
async def get_patient_age_of_facescan_by_id(
    facescan_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    facescans_repo: FacescansRepository = Depends(get_repository(FacescansRepository)),
) -> float:
    facescan_age = await facescans_repo.get_patient_age_at_time_of_facescan(facescan_id=facescan_id, requesting_user=current_user)
    if not facescan_age:
        raise HTTPException(status_code=HTTP_404_NOT_FOUND, detail="No facescan found with that id.")
    return facescan_age


@router.get("/vtkXMLPolyDataMesh/{facescan_id}", name="facescans:get-vtkxmlpolydata-mesh-by-facescan-id")
async def get_vtkxmlpolydata_by_facescan_id(
    facescan_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    facescans_repo: FacescansRepository = Depends(get_repository(FacescansRepository)),
) -> FacescanPublic:
    facescanmesh = await facescans_repo.get_vtkxmlpolydata_mesh(id=facescan_id, requesting_user=current_user)
    if not facescanmesh:
        raise HTTPException(status_code=HTTP_404_NOT_FOUND, detail="No facescan mesh found with that id.")
    return facescanmesh


"""
Creates a new facescan record for an existing patient.
Face mesh and texture need to be submitted separately through facescans PUT endpoints, referencing 
patient_id and the facescan ID returned by this POST endpoint.
"""
@router.post("/{patient_id}/facescans", response_model=FacescanPublic, name="facescans:create-facescan", status_code=HTTP_201_CREATED)
async def create_new_facescan(
    patient_id: int = Path(..., ge=1, title="Patient ID."),
    new_facescan: FacescanCreate = Body(..., embed=True),
    current_user: UserInDB = Depends(get_current_active_user),
    #meshfile: UploadFile = File(...),
    #texturefile: UploadFile = File(...),
    facescans_repo: FacescansRepository = Depends(get_repository(FacescansRepository))
) -> FacescanPublic:
    #new_facescan['patient_id']=patient_id
    created_facescan = await facescans_repo.create_facescan(new_facescan=new_facescan, patient_id = patient_id, requesting_user=current_user)
    return created_facescan
    

@router.put("/facescanmesh/{facescan_id}", response_model=FacescanPublic, name="facescans:upload-facescan-mesh", 
    status_code=HTTP_201_CREATED, dependencies=[Depends(check_facescan_modification_permissions)])
async def facescan_upload_mesh(
    facescan_id: int = Path(..., ge=1, title="Facescan ID."),
    current_user: UserInDB = Depends(get_current_active_user),            
    meshfile: UploadFile = File(...),
    facescans_repo: FacescansRepository = Depends(get_repository(FacescansRepository))
) -> FacescanPublic:

    facescanWithMeshRecord = await facescans_repo.upload_facescan_mesh(facescan_id=facescan_id, meshfile=meshfile, requesting_user=current_user)
    
    if not facescanWithMeshRecord:
        raise HTTPException(
            status_code=HTTP_404_NOT_FOUND, 
            detail="No facescan with that id exists.",
        )
    
    return facescanWithMeshRecord


@router.get("/facescanmesh/{facescan_id}", response_model=bytes, name="facescans:download-facescan-mesh", 
    status_code=HTTP_201_CREATED, dependencies=[Depends(check_facescan_modification_permissions)])
async def facescan_download_mesh(
    facescan_id: int = Path(..., ge=1, title="Facescan ID."),
    current_user: UserInDB = Depends(get_current_active_user),    
    facescans_repo: FacescansRepository = Depends(get_repository(FacescansRepository))    
) -> bytes:

    facescanMesh = await facescans_repo.download_facescan_mesh(facescan_id=facescan_id, requesting_user=current_user)
    
    if not facescanMesh:
        raise HTTPException(
            status_code=HTTP_404_NOT_FOUND, 
            detail="No facescan mesh with that id exists.",
        )
    
    return Response(content=facescanMesh.mesh, media_type="application/octet-stream") 
    
    
@router.put("/facescantexture/{facescan_id}", response_model=FacescanPublic, name="facescans:upload-facescan-texture", 
    status_code=HTTP_201_CREATED, dependencies=[Depends(check_facescan_modification_permissions)])
async def facescan_upload_texture(
    facescan_id: int = Path(..., ge=1, title="Facescan ID."),
    current_user: UserInDB = Depends(get_current_active_user),            
    texturefile: UploadFile = File(...),
    facescans_repo: FacescansRepository = Depends(get_repository(FacescansRepository))
) -> FacescanPublic:

    facescanWithTextureRecord = await facescans_repo.upload_facescan_texture(facescan_id=facescan_id, texturefile=texturefile, requesting_user=current_user)
    
    if not facescanWithTextureRecord:
        raise HTTPException(
            status_code=HTTP_404_NOT_FOUND, 
            detail="No facescan with that id exists.",
        )

    return facescanWithTextureRecord      


@router.get("/facescantexture/{facescan_id}", response_model=bytes, name="facescans:download-facescan-texture", 
    status_code=HTTP_201_CREATED, dependencies=[Depends(check_facescan_modification_permissions)])
async def facescan_download_texture(
    facescan_id: int = Path(..., ge=1, title="Facescan ID."),
    current_user: UserInDB = Depends(get_current_active_user),        
    facescans_repo: FacescansRepository = Depends(get_repository(FacescansRepository))
) -> bytes:

    facescanTexture = await facescans_repo.download_facescan_texture(facescan_id=facescan_id, requesting_user=current_user)
    
    if not facescanTexture:
        raise HTTPException(
            status_code=HTTP_404_NOT_FOUND, 
            detail="No facescan texture with that id exists.",
        )

    return Response(content=facescanTexture.texture, media_type="application/octet-stream")  

    
    
"""    
@router.delete("/{patient_id}/facescans/{facescan_id}/", response_model=int, name="facescans:delete-facescan-by-id")
async def delete_facescan_by_id(
    facescan_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    facescans_repo: FacescansRepository = Depends(get_repository(FacescansRepository)),
) -> int:

    deleted_id = await cleanings_repo.delete_facescan_by_id(id=facescan_id, requesting_user=current_user)
    
    if not deleted_id:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="No facescan found with that id.")
        
    return deleted_id
"""        
@router.delete(
    "/{patient_id}/facescans/{facescan_id}/",
    response_model=int,
    name="facescans:delete-facescan-by-id",
    dependencies=[Depends(check_facescan_modification_permissions)],
)
async def delete_facescan_by_id(
    facescan: FacescanInDB = Depends(get_facescan_by_id_from_path),
    facescans_repo: FacescansRepository = Depends(get_repository(FacescansRepository)),
) -> int:
    return await facescans_repo.delete_facescan_by_id(facescan=facescan)
    
