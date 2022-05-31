# FASDashboard
Public API for FAS clinical interface, landmarking server and geometry processing. 

Repository does not include code for Frontend demo.


## Deployment

- Install Docker, and add username to group 'docker'.

- Checkout repository.

- Copy the following files currently not contained in this repository:

```
├── landmarker
│   │   ├── data
│   │   │   ├── models
│   │   │   │   ├── dlib
│   │   │   │   │   └── shape_predictor_68_face_landmarks.dat
│   │   │   │   └── zeyuCNN
│   │   │   │       ├── build_HG_e50_b8.hdf5
│   │   │   │       ├── build_HG.hdf5
│   │   │   │       ├── tf_hm_e50_b8.hdf5
│   │   │   │       ├── tf_hm_e80_b8_new.hdf5
│   │   │   │       ├── tf_hm_e80_b8_portrait_5point.hdf5
│   │   │   │       └── tf_hm_e80_b8_portrait.hdf5
.....
├── processor
│   ├── API_documentation.md
│   ├── Caucasian16pts
│   │   ├── isolated_face_model_665_CAUCASIAN_CONTROLS+alc exposed_16pts.csv
│   │   ├── isolated_face_proj_665_CAUCASIAN_CONTROLS+alc exposed_16pts.csv
│   │   ├── model.dat
│   │   └── projection.csv
│   ├── Caucasian16pts_splitModels
│   │   ├── Face
│   │   │   ├── split01
.......
│   │   │   │   ├── model.csv
│   │   │   │   └── training.dat
│   │   │   └── split20
│   │   │       ├── model.csv
│   │   │       └── training.dat
│   │   ├── Malar
│   │   │   ├── hintonC1000.dat
│   │   │   ├── hintonC100.dat
│   │   │   ├── hintonC10.dat
│   │   │   ├── hintonC1.dat
......
│   ├── faceScreenServer
│   ├── launch
│   ├── modelDB.json
│   └── vtksrc
│       └── VTK-9.0.1.tar.gz
.....
```
File `modelDB.json` should correctly describe classification models here stored in directories Caucasian16pts or Caucasian16pts_splitModels.

- On the CLI, run `docker-compose up --build`

- Initialise database as described in document `backend/doc/220329_fasdDashboard-deployment.pdf`

- The OpenAPI is available at `http://0.0.0.0:8000/docs`


## Open issues 

- HTTPS protocol is not configured. Implement a certificate update procedure based on letsencrypt.org

- User signup is not restricted. Implement a procedure for verifying new users, e.g., based on email confirmation.

- The PostGRES database uses standard ports, standard DB usernames, and standard DB passwords. Configure individually!

- Configure Docker container processes to run without priveleges inside containers.

- Database transaction logging (for, e.g., auditing purposes) is not configured.

- Define a viable backup procedure and implement it.

- Reproducible method to obtain 2D CNN landmarking models is not available.

- Endpoint to accept Canfield data not yet available.

- Wavefront OBJ and PNG texture data need to be checked for compliance to data format, consistency of texture coordinates, and so forth.


## Further security considerations

- After initial deployment, it is advisable to configure a Firewall on the host system, passing only packets with port numbers corresponding to the public API (Port 8000).

- In production mode, do not run any services or software not essential for the operation of FASDashboard!


## Future developments

- Permit process for supporting landmarking in case the automatic method fails. This may include user submitted bounding boxes, or a sparse 2D annotation of easy to identify landmarks.

- Improve scalability of the processing infrastructure.

- Migrate processingServer (now behind MS CppRestSDK RestAPI) to a leaner Python wrapper of vtkSurfacePCA code (potentially reachable through a FastAPI interface). This, for instance, will make it easier to add further analysis features (or cleaner to implement these respectively).



