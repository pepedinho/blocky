CXX      := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Werror -O2 -g -Isrc -MMD -MP
SANFLAGS := -fsanitize=address,undefined

BUILD_DIR := build
TARGET    := $(BUILD_DIR)/kv_server

SRCS := src/main.cpp src/io_uring.cpp src/socket.cpp
OBJS := $(SRCS:src/%.cpp=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

GREEN := \033[1;32m
CYAN  := \033[1;36m
RED   := \033[1;31m
RESET := \033[0m

.PHONY: all clean debug

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	@printf "$(CYAN)[LINK] $@$(RESET)\n"
	@$(CXX) $(OBJS) -o $@
	@printf "$(GREEN)[+] Build successful ! -> $@$(RESET)\n"

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[CXX]  $<$(RESET)\n"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

debug: CXXFLAGS += $(SANFLAGS)
debug: clean $(TARGET)

clean:
	@printf "$(RED)[CLEAN] Removing $(BUILD_DIR)...$(RESET)\n"
	@rm -rf $(BUILD_DIR)

-include $(DEPS)
