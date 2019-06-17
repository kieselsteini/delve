CC=cc -O2 -Wall -Wextra -DDELVE_USE_READLINE
LIB=-lreadline
OBJ=delve.o
BIN=delve

default: $(OBJ)
	$(CC) -o $(BIN) $(OBJ) $(LIB)

clean:
	rm -f $(BIN) $(OBJ)
