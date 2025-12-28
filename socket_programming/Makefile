CXX = g++
CXXFLAGS = -Wall -Wextra
SRCS = $(wildcard *.cpp)
TARGETS = $(SRCS:.cpp=)

# 默认目标：编译所有程序
all: $(TARGETS)

# 模式规则：从 %.cpp 生成可执行文件 %
%: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

# 清理所有生成的可执行文件
clean:
	rm -f $(TARGETS)

# 显示所有可用的目标
list:
	@echo "Available targets:"
	@echo "  all      - Build all programs"
	@echo "  clean    - Remove all built programs"
	@echo "  list     - Show this help message"
	@echo "\nIndividual programs:"
	@for target in $(TARGETS); do \
		echo "  $$target"; \
	done

.PHONY: all clean list