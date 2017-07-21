fileman_opinfo_dist = [
	'e-module-fileman_opinfo.edj',
	'module.desktop',
]

fileman_opinfo_src = [
	'e_mod_main.c',
]

fileman_opinfo_dir = join_paths(dir_module_e, 'fileman_opinfo', module_arch)
if get_option('fileman-opinfo') == true
	config_h.set('USE_MODULE_FILEMAN_OPINFO', '1')

	install_data(fileman_opinfo_dist,
		install_dir: join_paths(dir_module_e, 'fileman_opinfo')
	)

        module_files += join_paths(fileman_opinfo_dir, 'fileman_opinfo.so')
	shared_module('fileman_opinfo',
                fileman_opinfo_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: fileman_opinfo_dir,
                install: true
        )
endif

