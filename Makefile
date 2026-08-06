CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -O2 -Iinclude
SRC := main.cpp src/lexer.cpp src/parser.cpp src/irbuilder.cpp src/codegen.cpp
OBJ := $(SRC:.cpp=.o)
BIN := thzc

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

test: $(BIN) FORCE
	@for f in tests/cases/*.js; do \
		echo "=== $$f ==="; \
		./$(BIN) "$$f" -o /tmp/out.cpp 2>&1 && \
		$(CXX) $(CXXFLAGS) -o /tmp/out /tmp/out.cpp 2>&1 && \
		echo "--- saída ---" && /tmp/out 2>&1; \
	done

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all test clean
FORCE: