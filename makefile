
INC= -I.
LIB= -lpthread

CC=gcc
CC_FLAG=-Wall -O3 -g

PRG= test_case
OBJ= $(patsubst %.c,%.o,$(wildcard *.c))

$(PRG):$(OBJ)
	$(CC) $(INC) -o $@ $(OBJ) $(LIB)
	
%.o:%.c
	$(CC) -c  $(CC_FLAG) $(INC) $< -o $@

.PRONY:clean
clean:
	@echo "Removing linked and compiled files......"
	rm -f $(OBJ) $(PRG)
