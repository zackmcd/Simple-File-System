# Target library
lib  := libfs.a
objs := disk.o fs.o

CC   := gcc 
CFLAGS := -Wall -Werror
CFLAGS += -g

AA      := ar
AFLAGS  := -crs

ifneq ($(V),1)
Q = @
endif

all: $(lib)

# Dep tracking *must* be below the 'all' rule
deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

libfs.a : $(objs)
	@echo "CC $@"
	$(Q)$(AA) $(AFLAGS) $(lib) $(objs)

%.o : %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

clean :
	@echo "clean"
	$(Q)rm -f $(lib) $(objs) $(deps)
