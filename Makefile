include Makefile.include 

TARGET_LIBS= -L$(DEV_LIB_DIR) 

TARGET_INC= -I include $(DEV_INC_DIR)

all: 
	cd src; make all

clean:
	cd src ; make clean ; cd ../test ; make clean

test: all
	cd test ; make test

tags:
	$(TAGGEN)
    
# include make.targets 

