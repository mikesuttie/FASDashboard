from typing import List

from fastapi import APIRouter, Body, Depends
from starlette.status import HTTP_201_CREATED

from app.models.ethnicity import EthnicityCreate, EthnicityPublic
from app.db.repositories.ethnicities import EthnicitiesRepository
from app.api.dependencies.database import get_repository

router = APIRouter()

@router.get("/")
async def get_all_ethnicities(ethnicities_repo: EthnicitiesRepository = Depends(get_repository(EthnicitiesRepository))) -> List[dict]:
    ethnicities = await ethnicities_repo.get_all_ethnicities()    
    return ethnicities


@router.post("/", response_model=EthnicityPublic, name="ethnicities:create-ethnicity", status_code=HTTP_201_CREATED)
async def create_new_ethnicity(
    new_ethnicity: EthnicityCreate = Body(..., embed=True),
    ethnicities_repo: EthnicitiesRepository = Depends(get_repository(EthnicitiesRepository)),
) -> EthnicityPublic:
    created_ethnicity = await ethnicities_repo.create_ethnicity(new_ethnicity=new_ethnicity)
    return created_ethnicity
