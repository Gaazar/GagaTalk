XX = g++
CC = gcc
CFLAGS = -lpthread -ldl 
 
TARGET  = gts_build
OBJ_DIR = ./objs
SRC_DIR = ./src
 
SRC = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp, %.o, $(SRC))
 
VPATH = $(SRC_DIR)
vpath %.o $(OBJ_DIR)
 
$(info $(SRC))

$(info $(OBJECTS))

all: $(TARGET)
 
$(TARGET) : $(OBJECTS) sqlite3.o
	$(XX) -o $@ $(addprefix $(OBJ_DIR)/, $(OBJECTS)) objs/sqlite3.o $(CFLAGS)
 
%.o : %.cpp
	$(XX) -c -g $< -o $(OBJ_DIR)/$@ -I src/ 
 
sqlite3.o : src/sqlite/sqlite3.c 
	$(CC) -c $< -o objs/sqlite3.o

dep : $(TARGET)
	rm gts
	mv gts_build gts
run : dep
	nohup stdbuf -i0 -o0 -e0 ./gts > latest.log 2>&1 &
.PHONY : clean
clean:
	rm -rf $(TARGET) $(OBJ_DIR)/*.o ./*.o
