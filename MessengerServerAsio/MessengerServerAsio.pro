TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    MsngrSrv.cpp \
    tcpserver.cpp

LIBS += -lboost_filesystem -lboost_thread\
        -lmysqlcppconn8 -lssl -lcrypto -lresolv

unix {
LIBS += -pthread
}

HEADERS += \
    tcpserver.h
