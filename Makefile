# NDEJJE FARMERS SACCO - Loan Management Information System
# Build on Linux / macOS (or Windows with MinGW make):   make
# Run:                                                   ./NdejjeSACCO
# Remove the built program:                              make clean

CXX      ?= g++
CXXFLAGS  = -std=c++11 -Wall -Wextra -O2
SOURCES   = $(wildcard *.cpp)
HEADERS   = $(wildcard *.h)

NdejjeSACCO: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES)

clean:
	rm -f NdejjeSACCO NdejjeSACCO.exe

.PHONY: clean
