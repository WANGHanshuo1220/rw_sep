INC_DIR = ./include
BIN_DIR = ./bin
SRC_DIR = ./src
OBJ_DIR = ./obj
MBM_DIR = ./microbenchmark

SRC = $(wildcard $(SRC_DIR)/*.cpp)
MBM = $(wildcard $(MBM_DIR)/*.cpp)
OBJ1 = ${patsubst %.cpp, $(OBJ_DIR)/%.o, ${notdir ${SRC}}} 
OBJ2 = ${patsubst %.cpp, $(OBJ_DIR)/%.o, ${notdir ${MBM}}} 

#用于查看变量的值
#test:
#	@echo $(SRC)
#	@echo $(MBM)
#	@echo $(OBJ)

TARGET = lat
BIN_TARGET = ${BIN_DIR}/${TARGET}

CC = gcc
CFLAGS = -g -I${INC_DIR}

${BIN_TARGET}: ${OBJ1} $(OBJ2)
	${CC} ${OBJ} -o $@

$(OBJ1): $(SRC) 
	${CC} ${CFLAGS} -c $< -o $@

$(OBJ2): $(MBM) 
	${CC} ${CFLAGS} -c $< -o $@

clean:
	rm ./obj/*.o
