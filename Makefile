CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -O2
TARGET  = alu_fees
SRCS    = src/Main.c src/blockchain.c src/sha256.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) data/chain.bin data/pending.bin

.PHONY: all clean