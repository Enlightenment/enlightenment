dir_config = join_paths(dir_data, 'enlightenment/data/config')

eet = find_program('eet')
gen_eet = generator(eet,
                    output   : '@BASENAME@.cfg',
                    arguments: ['-e', '@OUTPUT@', 'config', '@INPUT@', '1'])

custom_target('profile.src',
              input      : 'profile.src',
              command    : [eet, '-i', '@OUTPUT@', 'config', '@INPUT@', '1'],
              output     : ['profile.cfg'],
              install    : true,
              install_dir: dir_config
             )
out = join_paths(dir_config, 'profile.cfg')
meson.add_install_script(chmod, 'a+r', out)

subdir('default')
subdir('mobile')
subdir('standard')
subdir('tiling')
