#!/bin/sh

###############################################################################
# 定义Qt目录
QT_DIR=/opt/Qt6.2.0/6.2.0/gcc_64
###############################################################################


###############################################################################
# 定义版本号
QFSVIEWER_MAJARVERSION=$(< ./version.txt cut -d '.' -f 1)
QFSVIEWER_SUBVERSION=$(< ./version.txt cut -d '.' -f 2)
QFSVIEWER_REVISION=$(< ./version.txt cut -d '.' -f 3)
export PATH=$QT_DIR/bin:$PATH
export LD_LIBRARY_PATH=$QT_DIR/lib:$LD_LIBRARY_PATH
export QT_PLUGIN_PATH=$QT_DIR/plugins
export QML2_IMPORT_PATH=$QT_DIR/qml
# 合成版本号
QFSVIEWER_VERSION="V"$QFSVIEWER_MAJARVERSION$QFSVIEWER_SUBVERSION$QFSVIEWER_REVISION
# 编译
rm -rf .qmake.stash Makefile
$QT_DIR/bin/lrelease ./QFSViewer.pro
$QT_DIR/bin/qmake -makefile
make
cp -R ./test ./build_release/out/QFSViewer.app/Contents/Resources/test
cp ./tools/create-dmg/build-dmg.sh ./build_release/out/build-dmg.sh
cp ./tools/create-dmg/installer_background.png ./build_release/out/installer_background.png
cd ./build_release/out
# 打包
$QT_DIR/bin/macdeployqt QFSViewer.app
otool -L ./QFSViewer.app/Contents/MacOS/QFSViewer
./build-dmg.sh QFSViewer
cd ../../
mkdir dmgOut
cpu=$(sysctl -n machdep.cpu.brand_string)
ARCH="x86_64"
case $cpu in
  *Intel*) ARCH="x86_64" ;;
  *Apple*) ARCH="arm64" ;;
esac
cp ./build_release/out/QFSViewer.dmg ./dmgOut/QFSViewer_macos_"$QFSVIEWER_VERSION"_"$ARCH".dmg
echo build success!
###############################################################################
