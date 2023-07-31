#!/bin/sh

###############################################################################
# 定义Qt目录
QT_DIR=/opt/Qt6.2.0/6.2.0/gcc_64

# 定义版本号
QFSVIEWER_MAJARVERSION="0"
QFSVIEWER_SUBVERSION="0"
QFSVIEWER_REVISION="2"
###############################################################################


###############################################################################
export PATH=$QT_DIR/bin:$PATH
export LD_LIBRARY_PATH=$QT_DIR/lib:$LD_LIBRARY_PATH
export QT_PLUGIN_PATH=$QT_DIR/plugins
export QML2_IMPORT_PATH=$QT_DIR/qml
# 合成版本号
QFSVIEWER_VERSION="V"$QFSVIEWER_MAJARVERSION$QFSVIEWER_SUBVERSION$QFSVIEWER_REVISION
# 编译
rm -rf .qmake.stash Makefile
qmake ./QFSViewer.pro -spec linux-g++ CONFIG+=qtquickcompiler
make clean
make -j8 
# clean打包目录
rm -rf ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64 
rm -f ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64.deb
# 构建打包目录
cp -r ./dpkg/QFSViewer ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64
# 使用linuxdeployqt拷贝依赖so库到打包目录
export QMAKE=$QT_DIR/bin/qmake
./tools/linuxdeploy-x86_64.AppImage --executable=./build_release/out/QFSViewer --appdir=./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/opt --plugin=qt
rm -rf ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/opt/apprun-hooks
mv ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/opt/usr ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/opt/QFSViewer
mv ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/opt/QFSViewer/bin/QFSViewer ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/opt/QFSViewer/QFSViewer
mv ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/opt/QFSViewer/bin/qt.conf ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/opt/QFSViewer/qt.conf
rm -rf ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/opt/QFSViewer/bin
sed -i "s/Prefix = ..\//Prefix = .\//g" ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/opt/QFSViewer/qt.conf
chrpath -r "\$ORIGIN/./lib" ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/opt/QFSViewer/QFSViewer
rm -rf ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/opt/QFSViewer/share
cp ./img/ico.png ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/opt/QFSViewer/QFSViewer.png
mkdir -p ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/opt/QFSViewer
cp -r ./test ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/opt/QFSViewer/test
# 配置打包信息
sed -i "s/#VERSION#/$QFSVIEWER_MAJARVERSION.$QFSVIEWER_SUBVERSION$QFSVIEWER_REVISION/g" ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/DEBIAN/control
SIZE=$(du -sh -B 1024 ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64 | sed "s/.\///g")
InstalledSize=$SIZE
sed -i "s/#SIZE#/$InstalledSize/g" ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/DEBIAN/control
chmod 755 ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64/* -R
# 打包
dpkg -b ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64 ./dpkg/QFSViewer_Linux_"$QFSVIEWER_VERSION"_x86_64.deb
echo build success!
###############################################################################
