CFLAGS := -Wall -g -O2 -D_GNU_SOURCE -fsanitize=thread
LDFLAGS := -lpthread

ALL := test_pthread test_linux

ALL_M := test_pthread test_linux
ALL_S := test_qspthread test_qslinux

all: $(ALL_M) $(ALL_S)
.PHONY: all

test_%: main.c atomic.h  cond.h  futex.h  mutex.h  spinlock.h
	$(CC) -std=c11 $(CFLAGS) main.c -o $@ $(LDFLAGS)

test_pthread: CFLAGS += -DUSE_PTHREADS
test_linux: CFLAGS += -DUSE_LINUX

test_qs%: qsort_mt.c atomic.h  cond.h  futex.h  mutex.h  spinlock.h
	$(CC) -std=gnu11 $(CFLAGS) qsort_mt.c -o $@ $(LDFLAGS)

test_qspthread: CFLAGS += -DUSE_PTHREADS
test_qslinux: CFLAGS += -DUSE_LINUX

# Test suite
NAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    PRINTF = printf
else
    PRINTF = env printf
endif
PASS_COLOR = \e[32;01m
NO_COLOR = \e[0m
notice = $(PRINTF) "$(PASS_COLOR)$(strip $1)$(NO_COLOR)\n"

check_main: $(ALL_M)
	@$(foreach t,$^,\
	    $(PRINTF) "Running $(t) ... "; \
	    ./$(t) && $(call notice, [OK]); \
	)

check_qsort: $(ALL_S)
	@$(foreach t,$^,\
	    $(PRINTF) "Running $(t) ... "; \
	    ./$(t) -tv && $(call notice, [OK]); \
	)

clean:
	$(RM) $(ALL_M) $(ALL_S)
.PHONY: clean