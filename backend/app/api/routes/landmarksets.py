from typing import List

from fastapi import APIRouter, Body, Depends
from starlette.status import HTTP_201_CREATED

from app.models.landmarkset import LandmarksetCreate, LandmarksetPublic
from app.db.repositories.landmarksets import LandmarksetsRepository
from app.api.dependencies.database import get_repository

router = APIRouter()

@router.get("/")
async def get_all_landmarksets(landmarksets_repo: LandmarksetsRepository = Depends(get_repository(LandmarksetsRepository))) -> List[dict]:
    landmarksets = await landmarksets_repo.get_all_landmarksets()    
    return landmarksets


"""
# TODO check:
# As of now, this post route doesn't work properly due to attribute binding issues with the JSON payload in the SQL query (writing to JSON typed DB column)
# Leaving it out for now (30Nov2021), as adding new landmarksets not often required. Writing them with alembic instead through the SQL alchemy interface, which works fine.

@router.post("/", response_model=LandmarksetPublic, name="landmarksets:create-landmarkse", status_code=HTTP_201_CREATED)
async def create_new_landmarkset(
    new_landmarkset: LandmarksetCreate = Body(..., embed=True),
    landmarksets_repo: LandmarksetsRepository = Depends(get_repository(LandmarksetsRepository)),
) -> LandmarksetPublic:
    created_landmarkset = await landmarksets_repo.create_landmarkset(new_landmarkset=new_landmarkset)
    return created_landmarkset
    
"""    
