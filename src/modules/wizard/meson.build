wizard_dist = [
	'data/def-ibar.txt',
	'data/desktop/home.desktop',
	'data/desktop/root.desktop',
	'data/desktop/tmp.desktop'
]

wizard_src = [
	'e_mod_main.c',
	'e_wizard.c',
	'e_wizard.h',
]

wizard_mods = [
	'page_000',
	'page_010',
	'page_011',
	'page_020',
	'page_030',
	'page_040',
	'page_050',
	'page_060',
	'page_065',
	'page_070',
	'page_080',
	'page_090',
	'page_100',
	'page_110',
	'page_120',
	'page_130',
	'page_150',
	'page_160',
	'page_170',
	'page_180',
	'page_200',
]

wizard_dir = join_paths(dir_module_e, 'wizard', module_arch)
if get_option('wizard') == true
	config_h.set('USE_MODULE_WIZARD', '1')

	install_data(wizard_dist,
		install_dir: join_paths(dir_module_e, 'wizard')
	)

        module_files += join_paths(wizard_dir, 'wizard.so')
	shared_module('wizard',
                wizard_src,
                include_directories: include_directories(module_includes),
                name_prefix: '',
                dependencies: module_deps,
                install_dir: wizard_dir,
                install: true
        )

	foreach wizard_mod: wizard_mods
		shared_module(wizard_mod,
				'@0@.c'.format(wizard_mod),
				include_directories: include_directories(module_includes),
				name_prefix: '',
				dependencies: module_deps,
				install_dir: wizard_dir,
				install: true
		)
	endforeach
endif

