# If libtiff is installed in a nonstandard location you must edit
# TIFFPATH and uncomment the following three lines.
#TIFFPATH=$(HOME)/libtiff
#TIFFINC=-I$(TIFFPATH)/include
#TIFFLIB=-L$(TIFFPATH)/lib

# If libgeotiff is installed in a nonstandard location you must edit
# GEOTIFFPATH and uncomment the following three lines.
#GEOTIFFPATH=$(SETSMHOME)/libgeotiff-1.4.2
#GEOTIFFINC=-I$(GEOTIFFPATH)/include
#GEOTIFFLIB=-L$(GEOTIFFPATH)/lib

MPIFLAGS = -DBUILDMPI

INCS = $(TIFFINC) $(GEOTIFFINC)
LDFLAGS = $(TIFFLIB) $(GEOTIFFLIB)

OBJS = setsmgeo.o grid.o grid_triangulation.o edge_list.o
HDRS = Typedefine.hpp setsm_code.hpp setsmgeo.hpp grid_triangulation.hpp grid_types.hpp grid_iterators.hpp basic_topology_types.hpp


ifeq ($(COMPILER), intel)
  CC?=icc
  CXX?=icpc
  MPICC?=mpicc
  MPICXX?=mpicxx
  CFLAGS=-std=c99 -qopenmp
  CXXFLAGS=-std=c++11 -qopenmp
  OPTFLAGS?=-O3 -xHost
else
  CC?=gcc
  CXX?=g++
  MPICC?=mpicc
  MPICXX?=mpicxx
  CFLAGS=-std=c99 -fopenmp
  CXXFLAGS=-std=c++11 -fopenmp
  OPTFLAGS?=-O3 -ffast-math -march=native
endif

setsm : setsm_code.o $(OBJS)
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -o setsm setsm_code.o $(OBJS) $(LDFLAGS) -lm -lgeotiff -ltiff

setsm_mpi : setsm_code_mpi.o $(OBJS)
	$(MPICXX) $(CXXFLAGS) $(OPTFLAGS) $(MPIFLAGS) -o setsm_mpi setsm_code_mpi.o $(OBJS) $(LDFLAGS) -lm -lgeotiff -ltiff

setsm_code.o : setsm_code.cpp $(HDRS)
	$(CXX) -c $(CXXFLAGS) $(OPTFLAGS) $(INCS) setsm_code.cpp -o setsm_code.o

setsm_code_mpi.o : setsm_code.cpp $(HDRS)
	$(MPICXX) -c $(CXXFLAGS) $(OPTFLAGS) $(MPIFLAGS) $(INCS) setsm_code.cpp -o setsm_code_mpi.o

$(OBJS) : $(HDRS)

%.o : %.c
	$(CC) -c $(CFLAGS) $(OPTFLAGS) $(INCS) $< -o $@

%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $(OPTFLAGS) $(INCS) $< -o $@

.PHONY: clean

clean :
	rm -f setsm setsm_mpi
	rm -f *.o
