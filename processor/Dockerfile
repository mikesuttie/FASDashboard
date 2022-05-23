FROM ubuntu:20.04



#RUN apt install cmake
RUN apt-get update
RUN apt-get upgrade -y
RUN apt-get install libosmesa-dev libcpprest-dev -y 
#libboost-all-dev

#RUN apt install software-properties-common -y
#RUN apt-add-repository 'deb https://apt.kitware.com/ubuntu/ bionic main'

RUN apt-get install cmake -y
RUN apt-get install -y build-essential g++ gcc gcc-multilib make


COPY . .


RUN mkdir build
WORKDIR /vtksrc
#RUN tar -xvf buildStaticReleaseWithMesaNoX.tar.gz
RUN tar -xvf VTK-9.0.1.tar.gz
RUN mkdir build
WORKDIR /vtksrc/build
##RUN cmake -C ../CMakeCache.txt ..

#WORKDIR /build
#RUN cmake ..
##RUN make 


EXPOSE 34568 34568
WORKDIR /
#RUN ./faceScreenServer 34568 modelDB.json http://127.0.0.1:

#ENTRYPOINT ["./faceScreenServer", "34568", "modelDB.json", "http://127.0.0.1:"]
#RUN tail -f /dev/null

#RUN make /app
#CMD python /app/app.py
