if ENABLE_PLUGIN

noinst_LTLIBRARIES += libplugin.la

noinst_HEADERS += \
	./plugin/callservicesmanager.h \
	./plugin/chathandler.h \
	./plugin/chatservicesmanager.h \
	./plugin/jamiplugin.h \
	./plugin/jamipluginmanager.h \
	./plugin/mediahandler.h \
	./plugin/pluginloader.h \
	./plugin/pluginmanager.h \
	./plugin/pluginpreferencesutils.h \
	./plugin/streamdata.h \
	./plugin/pluginsutils.h

libplugin_la_SOURCES = \
	./plugin/jamipluginmanager.cpp \
	./plugin/pluginloader.cpp \
	./plugin/pluginmanager.cpp \
	./plugin/pluginpreferencesutils.cpp \
	./plugin/pluginsutils.cpp \
	./plugin/chatservicesmanager.cpp \
	./plugin/callservicesmanager.cpp

libring_la_LIBADD += libplugin.la

endif
