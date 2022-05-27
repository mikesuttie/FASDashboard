from typing import List
from pydantic import Json
from fastapi import APIRouter, Body, Depends, HTTPException, Path, UploadFile, File#, Response
from starlette.status import HTTP_201_CREATED, HTTP_404_NOT_FOUND

from app.models.user import UserCreate, UserUpdate, UserInDB, UserPublic
from app.models.signature import SignatureCreate, SignaturePublic, SignatureInDB
from app.db.repositories.signatures import SignatureRepository
from app.api.dependencies.database import get_repository
from app.api.dependencies.auth import get_current_active_user
from app.api.dependencies.signatures import get_signature_by_id_from_path, check_signature_modification_permissions

import httpx

router = APIRouter()

@router.get("/", response_model=Json, name="signature:test-processing-server-connection")
async def test_processing_server_connection(
    #current_user: UserInDB = Depends(get_current_active_user),
):
    processingserver_response = httpx.get("http://facescreenprocessor:34568/faceScreen/processor/", timeout=1.0) # processingToken/ (don't leak a processing token through this unprotected route)
    #print("Test: processingserver_response **************** " , processingserver_response, processingserver_response.text)
    return processingserver_response


@router.get("/{landmark_id}/", response_model=List[SignaturePublic], name="signature:get-signatures-by-landmark-id")
async def get_signature_by_landmark_id(
    landmark_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    signature_repo: SignatureRepository = Depends(get_repository(SignatureRepository))
):
    signature_records = await signature_repo.get_all_signature_records_for_landmark_record(landmark_id = landmark_id, requesting_user = current_user)
    
    if signature_records is None or len(signature_records)==0:
        raise HTTPException(status_code=HTTP_404_NOT_FOUND, detail="No signature available.")
    return signature_records


@router.get("/heatmap/{signature_id}/", response_model = bytes , name="signature:get-heatmap-by-signature-id")  # response_model= SignatureHeatmapMeshInDB
async def get_heatmap_by_signature_id(
    signature_id: int = Path(..., ge=1),
    current_user: UserInDB = Depends(get_current_active_user),
    signature_repo: SignatureRepository = Depends(get_repository(SignatureRepository))
):
    heatmap_mesh_data = await signature_repo.download_heatmap_mesh(signature_id = signature_id, requesting_user = current_user)
    
    if heatmap_mesh_data is None or len(heatmap_mesh_data.heatmap)==0:
        raise HTTPException(status_code=HTTP_404_NOT_FOUND, detail="No heatmap available under this signature id.")
    return heatmap_mesh_data.heatmap


# Route for testing heatmap computation invoked through Open API; results written into DB.
@router.post("/compute/{landmark_id}/", response_model=int, name="signature:compute", status_code=HTTP_201_CREATED)
async def signature_create(
    signaturemodel_name: str = "CAUC16pt",
    facialregion_code: str = "FACE", 
    landmark_id: int = Path(..., ge=1, title="Landmark ID."),   
    current_user: UserInDB = Depends(get_current_active_user),
    signature_repo: SignatureRepository = Depends(get_repository(SignatureRepository)),
):

    
    insertedSignatureRecordID = await signature_repo.compute_signature(landmark_id = landmark_id, 
                                                                       signaturemodel_name = signaturemodel_name, 
                                                                       facialregion_code = facialregion_code, 
                                                                       requesting_user = current_user)
    return insertedSignatureRecordID


@router.post("/", response_model=int, name="signature:create-new-record", status_code=HTTP_201_CREATED)
async def signature_create(
    new_signature_record: SignatureCreate,
    #landmark_id: int = Path(..., ge=1, title="Landmark ID."),   
    #modelname: str,
    #facialregion: str, 
    #principal_components: Json,
    current_user: UserInDB = Depends(get_current_active_user),
    signature_repo: SignatureRepository = Depends(get_repository(SignatureRepository)),
):

    # Endpoint for initiating the signature computation.        
    # As decided in meeting on 5 May 2022, the computation of signatures will be computed through a new python interface based on Swig; work to be continued by Mingze.
    # While this is not yet functional, computation of signatures is triggered by a direct connection of frontend to the CppRestSDK processing server, and results are uploaded to the DB through POST/PUT endpoints implemented here.
    # The PUT endpoint is to upload the heatmap, other values are uploaded at creation time through the POST endpoint.

    insertedSignatureID = await signature_repo.create_signature_record(new_signature = new_signature_record, requesting_user = current_user)

    return insertedSignatureID    

#TODO check: signature ID param, meshfile (Upload) param!
@router.put("/{signature_id}/",
    response_model=int,
    name="signatures:upload-heatmap",
    dependencies=[Depends(check_signature_modification_permissions)]
)
async def upload_heatmap_by_signature_id(
    signature: SignatureInDB = Depends(get_signature_by_id_from_path),
    meshfile: UploadFile = File(...),
    current_user: UserInDB = Depends(get_current_active_user),    
    signature_repo: SignatureRepository = Depends(get_repository(SignatureRepository)),
) -> int:
    return await signature_repo.upload_heatmap_mesh(signature_id = signature.id, meshfile = meshfile, requesting_user= current_user) 


@router.delete(
    "/{signature_id}/",
    response_model=int,
    name="signatures:delete-by-signature-id",
    dependencies=[Depends(check_signature_modification_permissions)],
)
async def delete_signature_by_id(
    signature: SignatureInDB = Depends(get_signature_by_id_from_path),
    signature_repo: SignatureRepository = Depends(get_repository(SignatureRepository)),
) -> int:
    return await signature_repo.delete_signature_by_id(signature=signature)
    
