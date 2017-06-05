TARGET     = smtp-server

BUILD_DIR  = build

MODULES    = server iniparser md5 base64

SOURCES    = $(foreach MODULE, $(MODULES), $(wildcard $(MODULE)/*.cpp))
SOURCES    += $(foreach MODULE, $(MODULES), $(wildcard $(MODULE)/*.c))
make_path  = $(addsuffix $(1), $(addprefix $(2), $(notdir $(basename $(3)))))
OBJECTS    = $(call make_path,.o, $(BUILD_DIR)/obj/, $(SOURCES))

RESOURCES  = resources
CONFIG     = server.config
QUEUE      = queue
PREFIX     = /usr/local

.PHONY: all release clean

all: release

.PHONY: install

install: release
	mkdir -p $(PREFIX)$(DESTDIR)/sbin
	mkdir -p $(PREFIX)/etc/$(TARGET)
	cp $(BUILD_DIR)/bin/$(TARGET) $(PREFIX)$(DESTDIR)/sbin/
	cp $(RESOURCES)/$(CONFIG) $(PREFIX)/etc/$(TARGET)/
	cp -r $(QUEUE) $(PREFIX)/etc/$(TARGET)/
	cp $(RESOURCES)/*list.txt $(PREFIX)/etc/$(TARGET)/
	cp $(RESOURCES)/user* $(PREFIX)/etc/$(TARGET)/

.PHONY: uninstall

uninstall: 
	rm -f $(PREFIX)$(DESTDIR)/sbin/$(TARGET)
	rm -f -r $(PREFIX)/etc/$(TARGET)

release:
	mkdir -p $(BUILD_DIR)/bin
	mkdir -p $(BUILD_DIR)/obj
	mkdir -p $(BUILD_DIR)/deps
	for MODULE in $(MODULES); \
		do $(MAKE) -f default.mk release MODULE=$$MODULE; \
	done
	$(MAKE) $(BUILD_DIR)/bin/$(TARGET)

$(BUILD_DIR)/bin/$(TARGET): $(OBJECTS)
	g++ $^ -o $@

clean:
	rm -rf $(BUILD_DIR)
