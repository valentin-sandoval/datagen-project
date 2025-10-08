CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11
LDFLAGS = -pthread -lrt

SRC = src/datagen.c
BIN = bin/datagen

all: $(BIN)

$(BIN): $(SRC)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $(BIN) $(SRC) $(LDFLAGS)
	@echo "Compilación completa -> $(BIN)"

run: all
	@$(BIN) -n 4 -m 100 -o tests/salida_100.csv

verify: tests/salida_100.csv
	@./verify.awk tests/salida_100.csv

clean:
	rm -rf bin tests/*.csv *.csv
	@echo "Proyecto limpio"

.PHONY: all run verify clean
