# HydroCoupleComposer

HydroCoupleComposer is a cross platform GUI for creating HydroCouple coupled model compositions. It can be launched using MPI for compositions containing components that support MPI.

```
mpirun -np 3 ./HydroCoupleComposer -r inputfile.hcp --ng
```

Type the following command for more help. 

```bash
./HydroCoupleComposer -h
```

![HydroCoupleComposer Screenshot](resources/images/hydrocouplecomposerscreenshot.png)

## Depedencies

HydroCoupleComposer can be compiled using the QtCreator project file included with this project or in visual studio using the Qt Visual Studio AddIn. Work is ongoing to add a CMake make file. Modify the project file to make sure that it points to the appropriate libraries on the target machine. Compilation of HydroCoupleComposer requires the following frameworks and libraries:

* Qt 5.7.0
* MPI
* Graphviz
* OpenMP
* [QPropertyModel](https://github.com/HydroCouple/QPropertyModel)
* [HydroCoupleVis](https://github.com/HydroCouple/HydroCoupleVis)

## References
-------------------------------------------------------------------------------------------------------------------
Buahin, C. and J. Horsburgh, 2016. From OpenMI to HydroCouple: Advancing OpenMI to Support Experimental Simulations and Standard Geospatial Datasets. International Congress on Environmental Modelling and Software. Toulouse,          France. http://scholarsarchive.byu.edu/iemssconference/2016/Stream-A/11