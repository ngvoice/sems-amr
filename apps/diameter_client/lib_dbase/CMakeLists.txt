set (libdbase_SRCS
avp.c
diameter_msg.c
tcp_comm.c
)

IF(OPENSSL_FOUND)
	ADD_DEFINITIONS(-DWITH_OPENSSL)
ENDIF(OPENSSL_FOUND)

ADD_LIBRARY(libdbase STATIC ${libdbase_SRCS})
ADD_CUSTOM_COMMAND(TARGET libdbase POST_BUILD COMMAND cp ARGS liblibdbase.a libliblibdbase.a)

