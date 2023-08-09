CC = g++

LIB = -lpthread


PRG=server
OBJ=RequestManager.o NetworkManager.o main.o csapp.o


$(PRG):$(OBJ)
	$(CC) -g -o $@ $(OBJ) $(LIB)

.SUFFIXES: .cpp .o .hpp 
.cpp.o:
	$(CC) $(CC_FLAG) -g -c $*.cpp -o $*.o $(LIB)

.PRONY:clean

clean:
	@echo "Removing linked and compiled files"
	rm -f $(OBJ) $(PRG)
