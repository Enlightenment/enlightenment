dir_config = join_paths(dir_data, 'enlightenment/data/config')

custom_target('profile.src',
	input: 'profile.src',
	command: [eet, '-i', '@OUTPUT@', 'config', '@INPUT@', '1'],
	output: ['profile.cfg'],
	install: true,
	install_dir: dir_config
)

subdir('default')
subdir('mobile')
subdir('standard')
subdir('tiling')
