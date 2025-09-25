CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall
LDLIBS := -lsqlite3

TARGET := interactive_changeset
SRC := interactive_changeset.cpp

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: clean

