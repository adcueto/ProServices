# Copyright (c) 2020, Wind River Systems, Inc.
#
# MIT License
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the ""Software""), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE. $(CC) $(SB_INC) -o thermostat_backend thermostat_backend.c $(SB_LIB)

SB_ROOT=/opt/fsl-imx-xwayland/4.14-sumo/sysroots/aarch64-poky-linux/usr/crank/runtimes/linux-imx8yocto-armle-opengles_2.0-obj/
SB_INC=-I$(SB_ROOT)/include
SB_LIB=-L$(SB_ROOT)/lib -lgreio

#Linux users add this
CFLAGS+= -DGRE_TARGET_OS_linux
SB_LIB+= -lpthread -ldl 

SOURCES=combi_backend.c

OBJECTS=$(SOURCES:.c=.o)

PROGRAM=combi_backend.bin

.PHONY: all clean rebuild

all: combi_backend.c

clean:
	$(RM) $(PROGRAM)
	$(RM) $(OBJECTS)

rebuild: clean all

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(PROGRAM): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@




all: combi_backend.c

combi_backend: combi_backend.c
	$(CC) $(CFLAGS) $(SB_INC) -o $@ $^ $(SB_LIB)
