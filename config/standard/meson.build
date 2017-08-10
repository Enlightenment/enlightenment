dir = 'standard'
src = [
  'e',
  'e_bindings',
  'module.battery',
  'module.cpufreq',
  'module.fileman',
  'module.ibar',
  'module.ibox',
  'module.pager',
  'module.temperature',
  'module.conf'
]

##### boilerplate config build + install of icons/dirs
i = 0
foreach cd: src
  custom_target(' '.join(['config_dist', dir, cd]),
		input      : cd + '.src',
		output     : cd + '.cfg',
		command    : [eet, '-e', '@OUTPUT@', 'config', '@INPUT@', '1'],
		install    : true,
		install_dir: join_paths(dir_config, dir)
               )
  i += 1
endforeach
install_data([ 'enlightenment-' + dir + '.png',
               'profile.desktop'
             ],
             install_dir: join_paths(dir_config, dir)
            )
foreach cd: src
  o = join_paths(dir_config, dir, cd + '.cfg')
  meson.add_install_script(chmod, 'a+r', o)
endforeach
