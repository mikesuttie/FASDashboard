FROM continuumio/miniconda3

# Manually-specified variables were not used by the project:
#    OPENGL_gl_LIBRARY
#    PYTHON_INCLUDE_DIR
#    PYTHON_LIBRARY

# ENV PYTHONPATH=/usr/lib/x86_64-linux-gnu/python3.6/site-packages/:/usr/bin/
# ENV LD_LIBRARY_PATH=/usr/bin/


WORKDIR /landmarker

ENV PYTHONDONTWRITEBYTECODE 1
ENV PYTHONBUFFERED 1

# Create the environment:
COPY environment.yml .
RUN conda env create -f environment.yml

# Make RUN commands use the new environment:
SHELL ["conda", "run", "-n", "FasdLandmarker", "/bin/bash", "-c"]

# install system dependencies
RUN apt-get update \
  && apt-get -y install netcat gcc postgresql \ 
  && apt-get -y install libgl1-mesa-glx xvfb \
 # && apt-get clean \
  && rm -rf /var/lib/apt/lists/*
 # libgl1-mesa-glx xvfb dependcies: opengl library and virutal framebuffer for offscreen rendering

# install python dependencies
RUN pip install --upgrade pip
COPY ./requirements.txt /landmarker/requirements.txt
RUN pip install -r requirements.txt

COPY . /landmarker

WORKDIR /landmarker/FasdLandmarker
EXPOSE 7998
#ENTRYPOINT ["init.sh"]
ENTRYPOINT ["conda", "run", "--no-capture-output", "-n", "FasdLandmarker","xvfb-run", "uvicorn", "server:app", "--reload", "--workers", "1", "--host", "0.0.0.0", "--port", "7998"]
#ENTRYPOINT ["conda", "run", "--no-capture-output", "-n", "FasdLandmarker",             "uvicorn", "server:app", "--reload", "--workers", "1", "--host", "0.0.0.0", "--port", "7998"]
#ENTRYPOINT ["tail", "-f","/dev/null"]

#ENV DISPLAY :99
#RUN Xvfb :99 &
#ENTRYPOINT [ "conda", "run", "--no-capture-output", "-n", "FasdLandmarker", " uvicorn", "server:app", "--reload", "--workers", "1", "--host", "0.0.0.0", "--port", "7998"]

