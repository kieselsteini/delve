CC=cc -O2 -Wall -Wextra
OBJ=delve.o
BIN=delve

default: $(OBJ)
	$(CC) -o $(BIN) $(OBJ)

clean:
	rm -f $(BIN) $(OBJ)
