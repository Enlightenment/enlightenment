tasks_src = [
	'e_mod_config.c',
	'e_mod_main.c',
	'e_mod_main.h',
]

tasks_dir = join_paths(dir_module_e, 'tasks', module_arch)
if get_option('tasks') == true
	config_h.set('USE_MODULE_TASKS', '1')

        module_files += join_paths(tasks_dir, 'tasks.so')
	shared_module('tasks',
                tasks_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: tasks_dir,
                install: true
        )
endif

