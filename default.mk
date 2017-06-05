CXX        = g++
CC         = gcc
BUILD_DIR  = build

INCDIR     = decoder 

CXXFLAGS   = -I. -O3 -g -Wall
PREFIX     = $(BUILD_DIR)

INC        = -I$(INCDIR) -I/usr/local/include

OBJ_DIR    = $(PREFIX)/obj
DEP_DIR    = $(PREFIX)/deps

make_path  = $(addsuffix $(1), $(basename $(subst $(2), $(3), $(4))))
src_to_obj = $(call make_path,.o, $(MODULE), $(OBJ_DIR), $(1))

SRCMODULES := $(wildcard $(MODULE)/*.cpp)
SRCMODULES += $(wildcard $(MODULE)/*.c)
OBJMODULES := $(call src_to_obj, $(SRCMODULES))
DEPSMK     = $(DEP_DIR)/$(MODULE)_deps.mk

release: $(OBJMODULES)

$(OBJ_DIR)/%.o: $(MODULE)/%.cpp
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

$(OBJ_DIR)/%.o: $(MODULE)/%.c
	$(CC) $(CXXFLAGS) $(INC) -c $< -o $@ 

$(DEPSMK): Makefile
	for FILE in $(SRCMODULES); do \
		g++ -I. -MM -MT build/debug/obj/$$(echo $$FILE | \
			sed "s/\.[^.]*$$/.o/") $$FILE >> $(DEPSMK); \
		g++ -I. -MM -MT $(DEPSMK) $$FILE >> $(DEPSMK); \
	done

-include $(DEPSMK)
