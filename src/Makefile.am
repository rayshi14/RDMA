bin_PROGRAMS = applauncher

INCLUDES = -I. -I../nio

applauncher_SOURCES = AppLauncher.cpp AppLauncher.h NioServer.cpp NioServer.h NioClient.cpp NioClient.h \
IBenchmarkTask.cpp IBenchmarkTask.h RdmaServer.cpp RdmaServer.h RdmaClient.cpp RdmaClient.h \
BufferUtils.cpp BufferUtils.h StopWatch.cpp StopWatch.h ReadyToReceive.cpp ReadyToReceive.h  \
ControlUtils.cpp ControlUtils.h DataPlaneRdma.cpp

applauncher_LDADD = -L../nio/.libs -libverbs -lrdmacm -lcpprdma

