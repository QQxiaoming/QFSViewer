###############################################################################
#                                                                             #
# QFSViewer 工程文件                                                           #  
#                                                                             # 
###############################################################################

# 定义版本号路径
QFSVIEWER_VERSION="$$cat(./version.txt)"

###############################################################################

!versionAtLeast(QT_VERSION, 6.5.0) {
    message("Cannot use Qt $$QT_VERSION")
    error("Use Qt 6.5.0 or newer")
}

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

# 源文件配置
INCLUDEPATH += \
        -I . \
        -I ./src

SOURCES += \
        src/mainwindow.cpp \
        src/qfsviewer.cpp \
        src/fsviewmodel.cpp \
        src/configFile.cpp \
        src/treemodel.cpp

HEADERS += \
        src/mainwindow.h \
        src/qfsviewer.h \
        src/fsviewmodel.h \
        src/filedialog.h \
        src/configFile.h \
        src/treemodel.h

FORMS += \
        src/mainwindow.ui

RESOURCES += \
        res/resource.qrc \
        theme/dark/darkstyle.qrc \
        theme/light/lightstyle.qrc

TRANSLATIONS += \
    lang/qfsviewer_zh_CN.ts \
    lang/qfsviewer_ja_JP.ts \
    lang/qfsviewer_en_US.ts

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

    build_info.commands = $$quote("c:/Windows/system32/WindowsPowerShell/v1.0/powershell.exe -ExecutionPolicy Bypass -NoLogo -NoProfile -File \"$$PWD/tools/generate_info.ps1\" > $$PWD/build_info.inc")
}

unix:!macx:{
    QMAKE_RPATHDIR=$ORIGIN
    QMAKE_LFLAGS += -no-pie
    
    build_info.commands = $$quote("cd $$PWD && ./tools/generate_info.sh > build_info.inc")
}

macx:{
    QMAKE_RPATHDIR=$ORIGIN
    ICON = "img\ico.icns"

    build_info.commands = $$quote("cd $$PWD && ./tools/generate_info.sh > build_info.inc")
}

build_info.target = $$PWD/build_info.inc
build_info.depends = FORCE
PRE_TARGETDEPS += $$PWD/build_info.inc
QMAKE_EXTRA_TARGETS += build_info

###############################################################################

