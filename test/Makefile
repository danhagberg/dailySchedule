include ../Makefile.include
MT = ./make-tests.sh 

ifeq ($(TARGET_OS),linux) 
	TIME_OBJ=$(PROJ_OBJ_DIR)/timeRoutinesLinux.o
	TIME_LIBS=-lm
else
	TIME_OBJ=$(PROJ_OBJ_DIR)/timeRoutinesOsx.o
	TIME_LIBS=-framework IOKit -framework CoreServices 
endif

TARGET=AllTests

OBJS = AllTests.o CuTest.o scheduleTest.o 
LIBS = -L$(DEV_LIB_DIR) -L$(PROJ_LIB_DIR) -lschedule -ll -ly $(TIME_LIBS) 
TARGET_INC = -I $(PROJ_INC_DIR) $(DEV_INC_DIR)

include ../Makefile.targets

#AllTests: $(OBJS)
#	$(CC) -o AllTests $(OBJS) $(LIBS)

#AllTests.o: AllTests.c
#	$(CC) -c AllTests.c

#AllTests.c: scheduleTest.c
#	$(MT) > AllTests.c

#CuTest.o: CuTest.c
#	$(CC) -c CuTest.c

#scheduleTest.o: scheduleTest.c
#	$(CC) -c scheduleTest.c

test: AllTests
	-./AllTests

clean: local_clean

local_clean:
	$(RM) AllTests *.o 

