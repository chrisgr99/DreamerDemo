# DreamerDemo — see docs/design.md
#
#   make        build the plugin
#   make dev    build and write it straight into Rack's plugins folder
#
# There is no `make dist`. This plugin is never packaged and never submitted to the library: it
# drives the host's interface and runs command-line programs, neither of which belongs in
# something other people install.

RACK_DIR ?= ../Rack-SDK

SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += $(wildcard LICENSE*)

include $(RACK_DIR)/plugin.mk

# DEVELOPMENT INSTALL, and why it is not `make install`.
#
# `make install` copies a .vcvplugin PACKAGE into the plugins folder, and Rack unpacks it on
# startup. Copy one while Rack is running and it can be cleared at shutdown without ever being
# unpacked — so the next start loads the old build, having silently thrown the new one away.
#
# This writes the built files straight into the folder Rack loads, and removes any package that
# might otherwise overwrite them with something older. Quit Rack, run this, start Rack.
RACK_USER_DIR ?= $(HOME)/Library/Application Support/Rack2
PLUGIN_DIR = $(RACK_USER_DIR)/plugins-mac-arm64/$(SLUG)

# REMOVED THEN WRITTEN, never copied over. Copying onto an existing dylib rewrites the same file
# in place, and macOS holds that file's code signature against its cached pages: the pages change,
# the signature does not, and the kernel kills Rack the moment it loads it. Unlinking first means
# the new file is a new file, with nothing cached against it.
dev: $(TARGET)
	@rm -f "$(RACK_USER_DIR)/plugins-mac-arm64/"$(SLUG)-*.vcvplugin
	@codesign --force --sign - $(TARGET) 2>/dev/null || true
	@mkdir -p "$(PLUGIN_DIR)"
	@rm -f "$(PLUGIN_DIR)/plugin.dylib"
	@cp $(TARGET) "$(PLUGIN_DIR)/plugin.dylib"
	@cp plugin.json "$(PLUGIN_DIR)/"
	@cp LICENSE "$(PLUGIN_DIR)/" 2>/dev/null || true
	@xattr -c "$(PLUGIN_DIR)/plugin.dylib" 2>/dev/null || true
	@codesign -v "$(PLUGIN_DIR)/plugin.dylib" && echo "signature valid"
	@echo "installed to $(PLUGIN_DIR)"

.PHONY: dev
