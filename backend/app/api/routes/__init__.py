from fastapi import APIRouter
from app.api.routes.patients import router as patients_router
from app.api.routes.ethnicities import router as ethnicities_router
from app.api.routes.facescans import router as facescans_router
from app.api.routes.patientnotes import router as patientnotes_router
from app.api.routes.users import router as users_router
from app.api.routes.landmarksets import router as landmarksets_router
from app.api.routes.landmarks import router as landmarks_router
from app.api.routes.PFL import router as PFL_router
from app.api.routes.signatures import router as signatures_router
from app.api.routes.meanClassifications import router as meanClassifications_router

router = APIRouter()

router.include_router(users_router, prefix="/users", tags=["users"])
router.include_router(ethnicities_router, prefix="/ethnicities", tags=["ethnicities"])
router.include_router(landmarksets_router, prefix="/landmarksets", tags=["landmarksets"])
router.include_router(patients_router, prefix="/patients", tags=["patients"])
router.include_router(patientnotes_router, prefix="/patientnotes", tags=["patientnotes"])
router.include_router(facescans_router, prefix="/facescans", tags=["facescans"])
router.include_router(landmarks_router, prefix="/landmarks", tags=["landmarks"])

router.include_router(PFL_router, prefix="/PFL", tags=["PFL"])
router.include_router(signatures_router, prefix="/signatures", tags=["signatures"])
router.include_router(meanClassifications_router, prefix="/meanClassifications", tags=["meanClassifications"])
