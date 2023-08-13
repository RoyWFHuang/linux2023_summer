FLAGS=-std=gnu11 -Wall -lpthread

OUT= align_up.out qsort_mt.out
CC=gcc
TSAN = 1
ifeq ("$(ASAN)","1")
    FLAGS += -g -fsanitize=address -fno-omit-frame-pointer -fno-common
endif

ifeq ("$(TSAN)", "1")
    FLAGS += -g -fsanitize=thread
endif

all: $(OUT)
clean:
	rm -rf $(OUT)

%.out: %.c
	echo "$(CC) -o $@ $^ $(FLAGS)"
	@$(CC) -o $@ $^ $(FLAGS)
