# 自动备份程序编译脚本
# 使用方法: make

.PHONY: all clean

# 默认目标：清理并重新编译
all: clean
	@echo "=== 开始编译自动备份程序 ==="
	bash build_release.sh
	@echo "=== 编译完成！==="

# 清理编译产物
clean:
	@echo "=== 清理编译产物 ==="
	rm -rf build/ out/