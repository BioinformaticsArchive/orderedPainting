# ----------------------------------------------------------------------
#
# This makefile has been automatically generated by WinGDB 2.4, build 1584
# Do not edit.
#
# Project: randomize
#
# ----------------------------------------------------------------------

PLATFORM=unix

ifneq ("$(windir)","")
PLATFORM=windows
endif

ifneq ("$(WinDir)","")
PLATFORM=windows
endif

ifneq ("$(WINDIR)","")
PLATFORM=windows
endif

UNAME=$(shell uname -o)

ifeq ("$(UNAME)","Cygwin")
PLATFORM=cygwin
endif

ifeq ("$(BUILD_CONFIGURATION)","")
	BUILD_CONFIGURATION=Debug
endif

ifeq ("$(BUILD_CONFIGURATION)","Debug")
CC= gcc
CXX= g++
AR= ar
CFLAGS= -O3
CXXFLAGS= -O3
ARFLAGS= cru
LDFLAGS= 
RESULT=rd
INTERMEDIATE_DIR=.
DESTINATION_DIR=.
ifeq ("$(PLATFORM)","cygwin")
	RM=rm -f
	MKDIR=mkdir -p
	INTERMEDIATE_DIR_MK=$(INTERMEDIATE_DIR)
	DESTINATION_DIR_MK=$(DESTINATION_DIR)
else
	RM= rm
	MKDIR= mkdir
	INTERMEDIATE_DIR_MK=.
	DESTINATION_DIR_MK=.
endif

endif

SOURCES= main.cpp

SOURCEPATHS= main.cpp

HEADERS=

VPATH= $(INTERMEDIATE_DIR)

OBJECTNAMES=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCES)))
OBJECTPATHS=$(addprefix $(INTERMEDIATE_DIR)/,$(OBJECTNAMES))

$(DESTINATION_DIR)/$(RESULT): $(OBJECTNAMES)
ifeq ("$(wildcard $(DESTINATION_DIR) )","")
	-$(MKDIR) $(DESTINATION_DIR_MK)
endif
	$(CXX) $(CXXFLAGS) $(SYSROOTFLAGS) $(OBJECTPATHS) -o $@ $(LDFLAGS)

$(INTERMEDIATE_DIR)/randomize.d: $(SOURCES) $(HEADERS)
ifeq ("$(wildcard $(DESTINATION_DIR) )","")
	-$(MKDIR) $(DESTINATION_DIR_MK)
endif
ifeq ("$(wildcard $(INTERMEDIATE_DIR) )","")
	-$(MKDIR) $(INTERMEDIATE_DIR_MK)
endif
	$(CXX) $(CXXFLAGS) -MM $(SOURCEPATHS) > $@

-include $(INTERMEDIATE_DIR)/randomize.d

clean:
	$(RM) $(DESTINATION_DIR)/$(RESULT)
	$(RM) $(INTERMEDIATE_DIR)/randomize.d
	$(RM) $(OBJECTPATHS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $(INTERMEDIATE_DIR)/$@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $(INTERMEDIATE_DIR)/$@ $<
