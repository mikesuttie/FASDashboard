# FaceScreenServer 
---

FaceScreenServer implements a C++ based backend for processing facial scans, computing information on dysmorphysms.


## API
---

FaceScreenServer implements a C++ based backend for processing facial scans, computing information on dysmorphysms.


## API

The [API documentation](./API_documentation.md) provides examples on consuming the service with `curl`.

## Building FaceScreenServer on MS Windows


Required: 
- C++17 compiler (e.g., use Visual Studio 2019)
- CMake
- vcpkg package manager: https://github.com/Microsoft/vcpkg
- C++ REST SDK: https://github.com/microsoft/cpprestsdk
- VTK: https://github.com/Kitware/VTK

1. Install recent version of Visual studio and CMake

2. Install Microsofts package manager vcpkg (the -disableMetrics parameter in bootstrap suppresses MS 'telemetry'):
```
> git clone https://github.com/Microsoft/vcpkg.git
> cd vcpkg
> .\bootstrap-vcpkg.bat -disableMetrics
> .\vcpkg integrate install
```

3. Install c++ REST SDK using the package manager just installed. Don't try any other method of installation. It's likely a wast e of time.
```
> .\vcpkg install cpprestsdk cpprestsdk:x64-windows
```

4. Create a build directory where file CMakeLists of the FaceScreenServer repository is located and enter this directory. Invoke cmake:
```
> cd [path where CMakeLists is located]
> mkdir build
> cd build
> cmake .. -A x64 -DCMAKE_TOOLCHAIN_FILE=[path to vcpkg.cmake]
```

Example of invoking cmake:
```
> cmake .. -A x64 -DCMAKE_TOOLCHAIN_FILE=/c/Users/facescreen/vcpkg/scripts/buildsystems/vcpkg.cmake
```

5. Install any missing dependencies and rerun cmake until VS solution was generated.
Open Visual Studio solution and compile.


Note: 

To run report generation, a latex compiler is required. 
FaceScreenServer was tested on Windows with MikTex: https://miktex.org/
MikTex will ask for confirmation to automatically install packages graphicx and tikz on first latex invocation (i.e., when requesting FASD report generation).


- Open Issues: Offscreen rendering (using Mesa3D library) does not seem to be supported on Windows any more. This precludes rendering on basic Win instances (e.g., ec2).


## Building FaceScreenServer on Linux

1. Build VTK - preferably as a static library - with the following CMake settings to enable offscreen rendering by default:

- configure

- select VTK_OPENGL_HAS_MESA

- disable VTK_USE_X

- configure

- select VTK_DEFAULT_RENDER_WINDOW_OFFSCREEN

- configure

- generate

- make


2. Convincing cmake to produce make files that build c++17 code and other setting different to Windows builds:

- Install gcc (GNU compiler) version 8 and force cmake to use it:

`set (CMAKE_CXX_COMPILER /usr/bin/g++-8)`

Alternatively, if gcc >= version 8 is the default compiler. it's sufficient to set:

`set (CMAKE_CXX_STANDARD 17)`
`set (CMAKE_CXX_STANDARD REQUIRED ON)`

- Note: gcc fully supports C++17 from Version 6, but with cmake it only works without trouble starting with Version 8 - Perhaps because in prior versions some extra compiler flags need to be set to enable C++17 support.

- Boost libraries need to be added manually (in Windows the required module (boost threads) was part of the cpprestsdk installation via vckg package manager):

`find_package(Boost REQUIRED thread)`
`target_link_libraries(faceScreenServer PRIVATE  . . .  ${Boost_LIBRARIES}  . . . )`

- Using the std::filesystem functions from the C++17 standard requires linking an extra library (its name is stdc++fs) in Linux:

`target_link_libraries(faceScreenServer PRIVATE  . . .  stdc++fs  . . . )`

- A parameter needs to be passed to let cmake find the dependencies (on Windows this is the path to the vckg package manager database):

`CMAKE_PREFIX_PATH = /usr/lib/x86_64-linux-gnu/cmake`

Preferably do it on CLI (in build dir) when generating the build config (also see below in subheading 'Building'):

`cmake .. -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake`


3. Prior to building, it may be necessary to install some packages:

$ sudo apt-get install libosmesa-dev

$ sudo apt-get install libcpprest-dev

$ sudo apt-get install libboost-all-dev


4. Building
```
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake
make
```
