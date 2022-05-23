FROM python:3.8-slim-buster

WORKDIR /backend

ENV PYTHONDONTWRITEBYTECODE 1
ENV PYTHONBUFFERED 1

# install system dependencies
RUN apt-get update \
  && apt-get -y install netcat gcc postgresql \
  && apt-get clean

#xorg-libx11=1.7.2 - a (actually here unused) vtk dependency
RUN apt-get install -y libx11-dev
RUN apt-get install -y libgl1-mesa-glx

# install python dependencies
RUN pip install --upgrade pip
COPY ./requirements.txt /backend/requirements.txt
RUN pip install -r requirements.txt

COPY . /backend

