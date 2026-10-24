# Tycoon pour NumWorks
#   make        -> output/Tycoon.nwa (à installer sur my.numworks.com/apps)
#   make host   -> output/tycoon_host (version PC pour les tests)
Q ?= @
CC = arm-none-eabi-gcc
BUILD_DIR = output
export NODE_OPTIONS = --require $(CURDIR)/fixnav.js
NWLINK = npx --yes -- nwlink@1.0.0
EADK_CFLAGS := $(shell $(NWLINK) eadk-cflags-device)

CFLAGS = -std=c11 $(EADK_CFLAGS) -Os -Wall -Wextra -Wno-unused-parameter -fno-math-errno
CFLAGS += -fdata-sections -ffunction-sections -flto -fno-fat-lto-objects -fwhole-program -fvisibility=internal
CFLAGS += -fno-tree-loop-distribute-patterns
LDFLAGS = -Wl,--relocatable -nostartfiles --specs=nano.specs
LDFLAGS += -Wl,-e,main -Wl,-u,eadk_app_name -Wl,-u,eadk_app_icon -Wl,-u,eadk_api_level
LDFLAGS += -Wl,--gc-sections -flinker-output=nolto-rel

HEADERS = src/epsilon_app.h src/epsilon_files.h

.PHONY: build check host clean
build: $(BUILD_DIR)/Tycoon.nwa

check: $(BUILD_DIR)/Tycoon.nwa
	$(Q) $(NWLINK) nwa-bin $< $(BUILD_DIR)/Tycoon.bin
	@echo "BIN     $(BUILD_DIR)/Tycoon.bin: $$(wc -c < $(BUILD_DIR)/Tycoon.bin) octets"

$(BUILD_DIR)/main.o: src/main.c $(HEADERS) | $(BUILD_DIR)
	@echo "CC      $<"
	$(Q) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/icon.o: src/icon.png | $(BUILD_DIR)
	@echo "ICON    $<"
	$(Q) $(NWLINK) png-icon-o $< $@

$(BUILD_DIR)/Tycoon.nwa: $(BUILD_DIR)/main.o $(BUILD_DIR)/icon.o
	@echo "LD      $@"
	$(Q) $(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
	$(Q) arm-none-eabi-strip --strip-unneeded $@

host: $(BUILD_DIR)/tycoon_host
$(BUILD_DIR)/tycoon_host: src/main.c host/eadk_host.c $(HEADERS) | $(BUILD_DIR)
	@echo "HOST    $@"
	$(Q) cc -std=c11 -O1 -g -Wall -Wextra -Wno-unused-parameter -DHOST -DPLATFORM_DEVICE=1 \
	  $(filter -I%,$(subst ",,$(EADK_CFLAGS))) src/main.c host/eadk_host.c -o $@

$(BUILD_DIR):
	$(Q) mkdir -p $@

clean:
	$(Q) rm -rf $(BUILD_DIR)
