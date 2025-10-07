
OUT := emu
DIRS := src devices
SRCS := $(shell find $(DIRS) -name "*.c" -type f ! -name "framebuffer_*.c")
CFLAGS := -Iinclude -Wall -Wextra
LIBS := unicorn raylib
LDFLAGS := 
ifneq ($(DEBUG),)
	CFLAGS += -DDEBUG
	ifeq ($(DEBUG),m)
		CFLAGS += -DDEBUG_MEM
	endif
endif
OBJS := $(patsubst %.c,%.o,$(SRCS))
DEPS := $(patsubst %.c,%.d,$(SRCS))

all: getObjects emu

$(OUT): $(OBJS)
	$(CC) $^ $(CFLAGS) $(LDFLAGS) $(addprefix -l,$(LIBS)) -o $@

%.o: %.c
	$(CC) $< $(CFLAGS) -o $@ -c

%.d: %.c
	$(CC) -MM $< $(CFLAGS) -MT $(patsubst %.d,%.o,$@) -o $@


clean:
	rm -f $(DEPS) $(OBJS) $(OUT)

getObjects: $(DEPS)
include $(wildcard $(DEPS))
