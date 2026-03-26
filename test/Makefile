NAME		:= app

# CFLAGS		:= -Wall -Wextra
# CFLAGS += -g -fsanitize=address
IFLAGS		:= -I include -I ../common/


SRC_DIR		:= src
SRC_FILES	:= main.c
SRC_LIST	:= $(addprefix $(SRC_DIR)/, $(SRC_FILES))

OBJ_DIR		:= obj
OBJ_LIST	:= $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC_LIST))



all: $(NAME)


$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(IFLAGS) -o $@ -c $<

$(OBJ_DIR):
	mkdir $(OBJ_DIR)



.PHONY: clean
clean:
	rm -rf $(OBJ_DIR)

.PHONY: fclean
fclean: clean
	rm -f $(NAME)


.PHONY: compile_commands
compile_commands:
	$(MAKE) | compiledb

compile_commands_test:
	$(MAKE) test | compiledb

.PHONY: run
run: all
	./$(NAME)


TEST_SRC_FILES	:= test_samples.c
TEST_C_SRC_FILES:= ../common/samples.c
TEST_SRC_LIST	:= $(TEST_SRC_FILES)
TEST_BIN_DIR 	:= bin
TEST_RUNNER_DIR := runners
TEST_BIN_LIST	:= $(patsubst %.c, $(TEST_BIN_DIR)/%, $(TEST_SRC_LIST))

TEST_UNITY_ROOT	:= Unity
TEST_UNITY_SRC	:= $(TEST_UNITY_ROOT)/src/unity.c
TEST_IFLAGS		:= -I $(TEST_UNITY_ROOT)/src 
TEST_UNITY_OPTIONS := \
					-DUNITY_INCLUDE_PRINT_FORMATTED \
					-DUNITY_OUTPUT_COLOR

$(TEST_BIN_DIR):
	mkdir $@

$(TEST_RUNNER_DIR):
	mkdir -p $@

# Save all the generated runners for debugging.
PRECIOUS: $(patsubst %.c, $(TEST_RUNNER_DIR)/%_runner.c, $(TEST_SRC_FILES))
$(TEST_RUNNER_DIR)/%_runner.c: %.c | $(TEST_RUNNER_DIR)
	ruby $(TEST_UNITY_ROOT)/auto/generate_test_runner.rb $< $@

$(TEST_BIN_DIR)/%: $(TEST_RUNNER_DIR)/%_runner.c $(TEST_C_SRC_FILES) | $(TEST_BIN_DIR)
	$(CC) $(CFLAGS) $(IFLAGS) $(TEST_IFLAGS) $(TEST_UNITY_OPTIONS) $(TEST_UNITY_SRC) $*.c $< -o $@  $(TEST_C_SRC_FILES) -D UNITY_TESTING


test_%: $(TEST_BIN_DIR)/test_% 
	./$<

.PHONY: test
test: $(TEST_BIN_LIST)
	for test in $(TEST_BIN_LIST) ; do ./$$test -j1 ; done

