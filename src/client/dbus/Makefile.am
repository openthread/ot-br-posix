include $(abs_top_nlbuild_autotools_dir)/automake/pre.am

noinst_LTLIBRARIES = libotbr-client-dbus.la

bin_PROGRAMS = dbus-client-test

dbus_client_test_SOURCES = \
    main.cpp               \
    $(NULL)

dbus_client_test_CXXFLAGS = \
    -fno-exceptions         \
    $(NULL)

dbus_client_test_CPPFLAGS                                  = \
    -I$(top_srcdir)/src                                      \
    -I$(top_srcdir)                                          \
    -I$(top_srcdir)/third_party/openthread/repo/include      \
    $(DBUS_CFLAGS)                                           \
    $(NULL)

dbus_client_test_LDADD   = \
    libotbr-client-dbus.la \
    $(NULL)

dbus_client_test_LDFLAGS = \
    -static                \
    $(NULL)

libotbr_client_dbus_la_SOURCES = \
    thread_api_dbus.cpp          \
    $(NULL)

libotbr_client_dbus_la_LIBADD              = \
    $(top_builddir)/src/dbus/libotbr-dbus.la \
    $(top_builddir)/src/utils/libutils.la    \
    $(NULL)

libotbr_client_dbus_la_LDFLAGS = \
    -static                      \
    $(NULL)

libotbr_client_dbus_la_CPPFLAGS                           = \
    -I$(top_srcdir)/src                                     \
    -I$(top_srcdir)                                         \
    -I$(top_srcdir)/third_party/openthread/repo/include     \
    $(DBUS_CFLAGS)                                          \
    $(NULL)

libotbr_client_dbus_la_CXXFLAGS = \
    -fno-exceptions               \
    $(NULL)

noinst_HEADERS        = \
    thread_api_dbus.hpp \
    $(NULL)

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
