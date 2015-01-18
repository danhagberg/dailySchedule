include ../Makefile.include 

ifeq ($(TARGET_OS),linux) 
	TIME_OBJ=$(PROJ_OBJ_DIR)/timeRoutinesLinux.o
	TIME_LIBS=-lm
else
	TIME_OBJ=$(PROJ_OBJ_DIR)/timeRoutinesOsx.o
	TIME_LIBS=-framework IOKit -framework CoreServices 
endif

TARGET=$(PROJ_BIN_DIR)/dailySchedule

OBJS=$(PROJ_OBJ_DIR)/dailySchedule.o 

LIBOBJS=$(PROJ_OBJ_DIR)/schedule.o $(PROJ_OBJ_DIR)/scheduleParse.tab.o $(PROJ_OBJ_DIR)/scheduleParse.yy.o $(TIME_OBJ)

LIB=$(PROJ_LIB_DIR)/libschedule.a

LIBS= -L$(DEV_LIB_DIR) -L$(PROJ_LIB_DIR) -lschedule $(TIME_LIBS) -ll -ly

TARGET_INC= -I ../include $(DEV_INC_DIR)

include ../Makefile.targets 

all: $(TARGET)

$(PROJ_OBJ_DIR)/scheduleParse.tab.o: scheduleParse.tab.c 

$(PROJ_OBJ_DIR)/scheduleParse.yy.o: scheduleParse.yy.c scheduleParse.tab.h

scheduleParse.tab.c scheduleParse.tab.h: scheduleParse.y
	$(YACC) -d scheduleParse.y

scheduleParse.yy.c: scheduleParse.l
	$(LEX) -o scheduleParse.yy.c scheduleParse.l 

clean: local_clean

local_clean: 
	$(RM) dailySchedule $(PROJ_OBJ_DIR)/*.o scheduleParse.tab.h scheduleParse.tab.c scheduleParse.yy.c scheduleParse.output testTimeStamp
