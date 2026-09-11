CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -D_GNU_SOURCE -Isrc/include
LDFLAGS ?=

BIN := bin

.PHONY: all clean run

all: $(BIN)/vrchat $(BIN)/metadata $(BIN)/scan

$(BIN):
	mkdir -p $@

$(BIN)/vrchat: src/app/main.c src/platform/paths.c src/platform/process.c src/dumper/dumper.c | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BIN)/metadata: src/metadata/main.c src/metadata/names.c | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BIN)/scan: src/native/main.c src/native/pe.c src/native/il2cpp.c src/native/json.c src/arch/amd64/memfind.asm | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c,$^) -x assembler-with-cpp $(filter %.asm,$^) -x none $(LDFLAGS) -o $@

run: all
	./$(BIN)/vrchat

clean:
	rm -rf $(BIN)
