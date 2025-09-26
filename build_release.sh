#!/bin/sh

# builds a preset
build_preset() {
    echo Configuring $1 ...
    cmake --preset $1
    echo Building $1 ...
    cmake --build --preset $1
}

# 只构建 Switch 平台
build_preset switch

rm -rf out

# --- SWITCH --- #
echo "打包自动备份程序中..."
mkdir -p out/sysFtpAutoBack/config/ftpsrv
mkdir -p out/sysFtpAutoBack/atmosphere/contents/
cp assets/config.template.ini out/sysFtpAutoBack/config/ftpsrv/
cp -r build/switch/420000000000011B out/sysFtpAutoBack/atmosphere/contents/

echo "- 自动备份程序打包成功！"
echo "- 输出位置: out/sysFtpAutoBack/"
