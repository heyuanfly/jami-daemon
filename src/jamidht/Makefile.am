noinst_LTLIBRARIES += libringacc.la

libringacc_la_LIBADD=

AM_CPPFLAGS += -I$(top_srcdir)/src/jamidht/eth

libringacc_la_SOURCES = \
	./jamidht/abstract_sip_transport.h \
	./jamidht/jamiaccount.cpp \
	./jamidht/jamiaccount.h \
	./jamidht/connectionmanager.h \
	./jamidht/connectionmanager.cpp \
	./jamidht/channeled_transport.h \
	./jamidht/channeled_transport.cpp \
	./jamidht/channeled_transfers.h \
	./jamidht/channeled_transfers.cpp \
	./jamidht/conversation.h \
	./jamidht/conversation.cpp \
	./jamidht/conversationrepository.h \
	./jamidht/conversationrepository.cpp \
	./jamidht/gitserver.h \
	./jamidht/gitserver.cpp \
	./jamidht/channel_handler.h \
	./jamidht/conversation_channel_handler.h \
	./jamidht/conversation_channel_handler.cpp \
	./jamidht/conversation_module.h \
	./jamidht/conversation_module.cpp \
	./jamidht/multiplexed_socket.h \
	./jamidht/multiplexed_socket.cpp \
	./jamidht/accountarchive.cpp \
	./jamidht/accountarchive.h \
	./jamidht/p2p.cpp \
	./jamidht/p2p.h \
	./jamidht/jami_contact.h \
	./jamidht/contact_list.h \
	./jamidht/contact_list.cpp \
	./jamidht/account_manager.h \
	./jamidht/account_manager.cpp \
	./jamidht/archive_account_manager.h \
	./jamidht/archive_account_manager.cpp \
	./jamidht/server_account_manager.h \
	./jamidht/server_account_manager.cpp \
	./jamidht/sync_channel_handler.h \
	./jamidht/sync_channel_handler.cpp \
	./jamidht/sync_module.h \
	./jamidht/sync_module.cpp \
	./jamidht/transfer_channel_handler.h \
	./jamidht/transfer_channel_handler.cpp

if RINGNS
libringacc_la_SOURCES += \
	./jamidht/namedirectory.cpp \
	./jamidht/namedirectory.h
endif

include ./jamidht/eth/libdevcore/Makefile.am
include ./jamidht/eth/libdevcrypto/Makefile.am

libring_la_LIBADD += libringacc.la
