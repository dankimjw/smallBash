# sets default target to default instead of debug
.DEFAULT_GOAL := default

PROJ = smallsh
$(CC) = gcc
SRC  = main.c

OBJ = $(SRC:.c=.o)
# BIN = $(PROJ).bin
BIN = $(PROJ)
cflags.common := -std=c99
cflags.debug := -g -v -Wall -pedantic -std=gnu99
#cflags.debug :=  -g -v -Wall -pedantic
#CFLAGS = -std=c99
CFLAGS = -g -v -Wall -pedantic -std=gnu99

VOPT = --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --show-reachable=yes --keep-debuginfo=yes

.PHONY: default debug clean zip

# default: CFLAGS = -std=c99


default: $(BIN)
	@echo "DEFAULT Compilation. smallsh created.	$@"


debug: clean debug_block
	CFLAGS = $(cflags.debug)

debug_block: $(BIN)
	@valgrind $(VOPT) ./$(BIN)

$(BIN): $(OBJ)
	@echo "CC	$@"
	@$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	@echo "CC	$^"
	@$(CC) $(CFLAGS) -c $^

clean: $(CLEAN)
	@echo "RM	*.o"
	@echo "RM	$(BIN)"
	@rm -f *.o $(BIN) 

