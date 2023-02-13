QT += testlib
QT -= gui

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle

TEMPLATE = app

SOURCES +=  \
    main.cpp \
    tst_freshupdatetest.cpp \
    tst_hmacupdatetest.cpp \
    tst_partialupdatetest.cpp \
    tst_verifytest.cpp

HEADERS += \
    testfiles.h \

RESOURCES += \
    ressources.qrc

# dependecy LibTreeHash
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

# dependency quazip
unix:!macx: LIBS += -L$$PWD/../build/quazip/quazip/ -lquazip1-qt6

INCLUDEPATH += $$PWD/../quazip
DEPENDPATH += $$PWD/../quazip
