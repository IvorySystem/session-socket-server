#
# Gnu make file to build the Ivory Session Socket Server
#

ARCH = 32

ifdef ARM_ELF
TOOL_CHAIN=d:/cygwin/arm2_gcc3.4.0
UTIL_DIR=d:/cygwin/arm2_gcc3.4.0/bin
PROC=arm
TYPE=elf
INCPATH     = d:/cygwin/arm2_gcc3.4.0/arm-elf/include
INCGENERIC = -I. -I$(INCPATH) -Id:/cygwin/arm2_gcc3.4.0/lib/gcc/arm-elf/3.4.0/include
endif

ifndef ARM_ELF
PROC_TYPE=x86_64
else
PROC_TYPE=arm-elf
endif

ifndef CXX
ifdef ARM_ELF
CXX =$(TOOL_CHAIN)/bin/$(PROC)-$(TYPE)-g++
else
CXX = g++
endif
endif

ifeq ($(ARCH), 32)
   ARCH_FLAG = -m32
endif

DEP_FLAGS = -MMD -MP

ifndef CFLAGS
ifdef ARM_ELF
CFLAGS = -c -Os -MMD -mcpu=arm7tdmi -marm -fsigned-char -ffixed-lr  -mlong-calls -nostdinc $(INCGENERIC) -fno-builtin
else
ifdef DEBUG
CFLAGS = -c $(ARCH_FLAG) -g $(DEP_FLAGS) -Wno-conversion-null
else
CFLAGS = -c $(ARCH_FLAG) $(DEP_FLAGS) -Ofast -Wno-conversion-null
endif
endif
endif

ifdef ARM_ELF
DEFINES =  -DIVORY -DENABLE_ASSERT -DNO_STDIO -DMINIMAL_IOSTREAM -DWAVENET_OBA
else
DEFINES = -DIVORY -DENABLE_ASSERT
endif

objectDir = ../../../../build/gcc/$(PROC_TYPE)/obj/app/ivory/isss

libDir = ../../../../build/gcc/$(PROC_TYPE)/lib/API

executableDir = ../../../../build/gcc/$(PROC_TYPE)/app/ivory

includes = -I../../../header

sources = $(wildcard *.cpp)
objects :=

all:	$(executableDir)/isss

define makeObject =
$source = $(1)
object = $(objectDir)/$(source:.cpp=.o)
objects := $(objects) $$(object)
dependencyRule=$(objectDir)/$(source:.cpp=.d)
dependencies += $(dependencyRule)
$$(object): $$(source)
	$(CXX) $(DEPFLAGS) $(CFLAGS) $(DEFINES) $(includes) $$< -o $$@
-include $(dependencyRule)
endef

$(foreach source,$(sources),$(eval $(call makeObject,$(source))))

$(objectDir) :
	mkdir -p $(objectDir)

$(executableDir) :
	mkdir -p $(executableDir)

$(libDir)/gen.lib:
	cd ../../../API/gen && $(MAKE)

libraries = $(libDir)/gen.lib
 
$(executableDir)/isss:  $(objectDir) $(objects) $(libraries) $(executableDir)
	gcc $(ARCH_FLAG) $(objects) -L$(libDir) -lgen -lstdc++ -lm -o $@

clean:
	rm -rf $(objectDir)/*
	rm -rf $(executableDir)/*
