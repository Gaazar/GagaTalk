XX = g++
CC = gcc
CFLAGS = -lpthread -ldl
 
TARGET  = gts
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
	$(XX) -c $< -o $(OBJ_DIR)/$@ -I src/ 
 
sqlite3.o : src/sqlite/sqlite3.c 
	$(CC) -c $< -o objs/sqlite3.o


.PHONY : clean
clean:
	rm -rf $(TARGET) $(OBJ_DIR)/*.o ./*.o