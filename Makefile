CC      := gcc
CPP	:= g++
LIBS    := -lpthread
OUT_DIR := build
TARGETS := honeypot

.PHONY: $(TARGETS) clean install

all: $(TARGETS) | $(OUT_DIR)

$(OUT_DIR):
	mkdir $@

honeypot: honeypot.c | $(OUT_DIR)
	$(CC) $< -o $(OUT_DIR)/$@ $(LIBS)

clean:
	rm -r $(OUT_DIR)

install: $(OUT_DIR)
	cp $(OUT_DIR)/* /bin
