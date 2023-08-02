###############################################################################
#                                                                             #
# QFSViewer 工程文件                                                           #  
#                                                                             # 
###############################################################################
win32:{
    include(partform_win32.pri)
}

unix:{
    include(partform_unix.pri)
}

###############################################################################
# 定义需要的Qt组件
QT       += core gui
QT       += xml svg
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 编译配置
TARGET = QFSViewer
TEMPLATE = app
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += APP_VERSION="\\\"V$${QFSVIEWER_VERSION}\\\""
CONFIG += c++11

include(./lib/QFontIcon/QFontIcon.pri)
include(./lib/lwext4/lwext4.pri)
include(./lib/ff15/ff15.pri)
include(./lib/jffs2/jffs2.pri)

INCLUDEPATH += $${Z_DIR}\include
LIBS += -L $${Z_DIR}/lib

# 源文件配置
INCLUDEPATH += \
        -I . \
        -I ./src

SOURCES += \
        src/mainwindow.cpp \
        src/qfsviewer.cpp \
        src/configFile.cpp \
        src/treemodel.cpp

HEADERS += \
        src/mainwindow.h \
        src/qfsviewer.h \
        src/configFile.h \
        src/treemodel.h

FORMS += \
        src/mainwindow.ui

RESOURCES += \
        res/resource.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# 输出配置
build_type =
CONFIG(debug, debug|release) {
    build_type = build_debug
} else {
    build_type = build_release
}

DESTDIR     = $$build_type/out
OBJECTS_DIR = $$build_type/obj
MOC_DIR     = $$build_type/moc
RCC_DIR     = $$build_type/rcc
UI_DIR      = $$build_type/ui

# 平台配置
win32:{
    VERSION = $${QFSVIEWER_VERSION}.000
    RC_ICONS = "img\ico.ico"
    QMAKE_TARGET_PRODUCT = "QFSViewer"
    QMAKE_TARGET_DESCRIPTION = "QFSViewer based on Qt $$[QT_VERSION]"
    QMAKE_TARGET_COPYRIGHT = "GNU General Public License v3.0"

    git_tag.commands = $$quote("cd $$PWD && git describe --always --long --dirty --abbrev=10 --tags | $$PWD/tools/awk/awk.exe \'{print \"\\\"\"\$$0\"\\\"\"}\' > git_tag.inc")
}

unix:!macx:{
    QMAKE_RPATHDIR=$ORIGIN
    QMAKE_LFLAGS += -no-pie
    
    git_tag.commands = $$quote("cd $$PWD && git describe --always --long --dirty --abbrev=10 --tags | awk \'{print \"\\\"\"\$$0\"\\\"\"}\' > git_tag.inc")
}

macx:{
    QMAKE_RPATHDIR=$ORIGIN
    ICON = "img\ico.icns"

    git_tag.commands = $$quote("cd $$PWD && git describe --always --long --dirty --abbrev=10 --tags | awk \'{print \"\\\"\"\$$0\"\\\"\"}\' > git_tag.inc")
}

git_tag.target = $$PWD/git_tag.inc
git_tag.depends = FORCE
PRE_TARGETDEPS += $$PWD/git_tag.inc
QMAKE_EXTRA_TARGETS += git_tag

###############################################################################

