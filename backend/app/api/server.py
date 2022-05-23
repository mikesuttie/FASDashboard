from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.core import config, tasks

from app.api.routes import router as api_router

from fastapi_route_logger_middleware import RouteLoggerMiddleware
import logging

def get_application():
    app = FastAPI(title=config.PROJECT_NAME, version=config.VERSION)

    app.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )
    logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.DEBUG)

    app.add_middleware(RouteLoggerMiddleware)

    app.add_event_handler("startup", tasks.create_start_app_handler(app))
    app.add_event_handler("shutdown", tasks.create_stop_app_handler(app))

    app.include_router(api_router, prefix="/api")

    return app

#logging.config.fileConfig("./logging.conf")
#logging.basicConfig(filename='/tmp/fasdDashboardRestAPICall.log', level=logging.INFO)

app = get_application()
