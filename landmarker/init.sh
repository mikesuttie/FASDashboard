#!/bin/bash

conda run --no-capture-output -n FasdLandmarker
#xvfb-run
Xvfb :99 &
export DISPLAY=:99
uvicorn server:app --reload --workers 1 --host 0.0.0.0 --port 7998
