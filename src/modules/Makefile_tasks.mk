EXTRA_DIST += src/modules/tasks/module.desktop.in \
src/modules/tasks/e-module-tasks.edj
if USE_MODULE_TASKS
tasksdir = $(MDIR)/tasks
tasks_DATA = src/modules/tasks/e-module-tasks.edj \
	     src/modules/tasks/module.desktop


taskspkgdir = $(MDIR)/tasks/$(MODULE_ARCH)
taskspkg_LTLIBRARIES = src/modules/tasks/module.la

src_modules_tasks_module_la_LIBADD = $(MOD_LIBS)
src_modules_tasks_module_la_CPPFLAGS = $(MOD_CPPFLAGS)
src_modules_tasks_module_la_LDFLAGS = $(MOD_LDFLAGS)
src_modules_tasks_module_la_SOURCES = src/modules/tasks/e_mod_main.c \
			  src/modules/tasks/e_mod_main.h \
			  src/modules/tasks/e_mod_config.c

PHONIES += tasks install-tasks
tasks: $(taskspkg_LTLIBRARIES) $(tasks_DATA)
install-tasks: install-tasksDATA install-taskspkgLTLIBRARIES
endif
