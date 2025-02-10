VPATH = ./:../base/

ELF = pmap.exe
CFLAGS ?= -O2
CPPFLAGS = -I.

OBJS += eeprom-main.o eeprom.o elect.o elect-main.o mecha-main.o mecha.o updates.o platform-win.o
OBJS += main.o

# Define MinGW compiler
CC = gcc
RM = del /Q  # Use Windows delete command

# Add -DID_MANAGEMENT when ID_MANAGEMENT is defined
ifdef ID_MANAGEMENT
CPPFLAGS += -DID_MANAGEMENT
OBJS += eeprom-id.o id-main.o
endif

$(ELF): $(OBJS)
	$(CC) -o $(ELF) $(OBJS)

clean:
	$(RM) $(ELF) $(OBJS) eeprom-id.o id-main.o
