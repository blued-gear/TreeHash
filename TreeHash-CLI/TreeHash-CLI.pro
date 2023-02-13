QT -= gui

CONFIG += c++20 console
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../LibTreeHash/ -lLibTreeHash
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../LibTreeHash/ -lLibTreeHash
else:unix: LIBS += -L$$OUT_PWD/../LibTreeHash/ -lLibTreeHash

INCLUDEPATH += $$PWD/../LibTreeHash
DEPENDPATH += $$PWD/../LibTreeHash

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../LibTreeHash/libLibTreeHash.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../LibTreeHash/libLibTreeHash.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../LibTreeHash/LibTreeHash.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../LibTreeHash/LibTreeHash.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../LibTreeHash/libLibTreeHash.a
