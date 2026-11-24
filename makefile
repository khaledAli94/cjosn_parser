# gcc-host test module 
CC  = gcc
LD  = ld
OC  = objcopy
SZ  = size
OD  = objdump
NM  = nm
RE  = readelf

TARGET  = modtest

DEFINES = -D__HOST_PC_TEST__ 
CFLAGS  += $(DEFINES) -g3 -ggdb -Wall -O0 -ffunction-sections -fdata-sections -std=gnu99
ASFLAGS += $(DEFINES) -g3 -ggdb -Wall -O0 -ffunction-sections -fdata-sections -std=gnu99

LDFLAGS = -Wl,-Map=./$(TARGET).map -Wl,--gc-sections


H_DIRS  := $(sort $(dir $(shell find . -type f -name '*.h')))
INCDIRS := $(strip $(patsubst %, -I%, $(H_DIRS)))

SRC     = $(shell find . -type d) 
SFILES  := $(foreach dir, $(SRC), $(wildcard $(dir)/*.S))
CFILES  := $(foreach dir, $(SRC), $(wildcard $(dir)/*.c))

DEPS    := $(SFILES:.S=.o.d) $(CFILES:.c=.o.d)
OBJS    := $(SFILES:.S=.o) $(CFILES:.c=.o)

all: $(OBJS) link
    
link: $(OBJS)
	@echo [LD] linking $(TARGET)
	@$(CC) $(LDFLAGS) $^ -o $(TARGET) $(LIBS) -lgcc -lc -lm
	@$(SZ) $(TARGET)

$(SFILES:.S=.o): %.o: %.S 
	@echo [AS] $(notdir $<)
	@$(CC) $(ASFLAGS) $(INCDIRS) -c $< -o $@ -MD -MP -MF $@.d

$(CFILES:.c=.o): %.o: %.c
	@echo [CC] $(notdir $<)
	@$(CC) $(CFLAGS) $(INCDIRS) -c $< -o $@ -MD -MP -MF $@.d

clean:
	@echo cleaning up.. 
	@rm -f $(TARGET) $(shell find . -name "*.d" -o -name "*.map" -o -name "*.o")

.PHONY: all link clean