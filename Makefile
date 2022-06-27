PLUGIN   = stored-strings-plugin.so
GCC_PLUGINS_DIR := $(shell gcc -print-file-name=plugin)
PLUGIN_CFLAGS := -shared  -std=gnu++11 -I$(GCC_PLUGINS_DIR)/include -fPIC \
		  -O2 -Wall -fno-rtti -fno-exceptions -fasynchronous-unwind-tables \
		  -ggdb -Wno-narrowing -Wno-unused-variable \
		  -Wno-format-diag

all: $(PLUGIN)

$(PLUGIN): stored_strings_plugin.cpp
	g++ $(PLUGIN_CFLAGS) $< -o $@

clean:
	$(RM) *.{so,o,d}
