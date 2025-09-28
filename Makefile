#---------------------------------------------------------------------------------
# FtpAutoBack 项目统一编译脚本
# 用于顺序编译 ovl-FtpAutoBack 和 sys-FtpAutoBack 两个子项目
#---------------------------------------------------------------------------------

.PHONY: all clean ovl sys copy-outputs help
# 默认目标：编译所有项目并复制产物
all: copy-outputs
	@echo "=== 结束 ==="

# 编译 ovl-FtpAutoBack 项目（Tesla 覆盖层）
ovl:
	@echo "=== 开始编译 ovl-FtpAutoBack ==="
	@cd ovl-FtpAutoBack && $(MAKE) clean && $(MAKE) -j
	@echo "=== ovl-FtpAutoBack 编译完成 ==="

# 编译 sys-FtpAutoBack 项目（系统模块）
sys:
	@echo "=== 开始编译 sys-FtpAutoBack ==="
	@cd sys-FtpAutoBack && $(MAKE) -j
	@echo "=== sys-FtpAutoBack 编译完成 ==="

# 复制编译产物到统一输出目录（依赖于 ovl 和 sys 完成）
copy-outputs: ovl sys
	@echo "=== 正在执行复制编译产物到 out/ 目录 ==="
	@rm -rf out/
	@echo "=== 已清理旧的 out/ 目录 ==="
	@mkdir -p out/ovl-FtpAutoBack
	@echo "=== 已创建新的 out/ovl-FtpAutoBack 目录 ==="
	@mkdir -p out/sys-FtpAutoBack
	@echo "=== 已创建新的 out/sys-FtpAutoBack 目录 ==="
	@if [ -f ovl-FtpAutoBack/FtpAutoBack.ovl ]; then \
		cp ovl-FtpAutoBack/FtpAutoBack.ovl out/ovl-FtpAutoBack/; \
		echo "=== 已复制 FtpAutoBack.ovl ==="; \
	else \
		echo "*** 警告: 未找到 FtpAutoBack.ovl ***"; \
	fi
	@if [ -d sys-FtpAutoBack/out/sysFtpAutoBack ]; then \
		cp -r sys-FtpAutoBack/out/sysFtpAutoBack/* out/sys-FtpAutoBack/; \
		echo "=== 已复制 sys-FtpAutoBack 编译产物 ==="; \
	else \
		echo "*** 警告: 未找到 sys-FtpAutoBack 编译产物 ***"; \
	fi
	@echo "=== 所有编译产物复制完成 ==="

# 清理所有项目的编译产物
clean:
	@echo "=== 正在执行清理所有编译产物 ==="
	@cd ovl-FtpAutoBack && $(MAKE) clean 2>/dev/null || true
	@cd sys-FtpAutoBack && $(MAKE) clean 2>/dev/null || true
	@rm -rf out/
	@echo "=== 所有编译产物清理完成 ==="



# 显示帮助信息
help:
	@echo "FtpAutoBack 项目编译选项："
	@echo "  make           - 编译所有项目并复制产物到 out/ 目录"
	@echo "  make -j        - 并行编译所有项目并复制产物到 out/ 目录"
	@echo "  make clean     - 清理所有编译产物"
