gaudio_sources = [
  'gstburn.c',
  'gstchromium.c',
  'gstdilate.c',
  'gstdodge.c',
  'gstexclusion.c',
  'gstgaussblur.c',
  'gstsolarize.c',
  'gstplugin.c',
]

gaudio_headers = [
  'gstchromium.h',
  'gstgaussblur.h',
  'gstdilate.h',
  'gstexclusion.h',
  'gstsolarize.h',
  'gstdodge.h',
  'gstburn.h',
]

doc_sources = []
foreach s: gaudio_sources + gaudio_headers
  doc_sources += meson.current_source_dir() / s
endforeach

plugin_sources += {
  'gaudieffects': pathsep.join(doc_sources)
}

if get_option('gaudieffects').disabled()
  subdir_done()
endif

orcsrc = 'gstgaudieffectsorc'
if have_orcc
  orc_h = custom_target(orcsrc + '.h',
    input : orcsrc + '.orc',
    output : orcsrc + '.h',
    command : orcc_args + ['--header', '-o', '@OUTPUT@', '@INPUT@'])
  orc_c = custom_target(orcsrc + '.c',
    input : orcsrc + '.orc',
    output : orcsrc + '.c',
    command : orcc_args + ['--implementation', '-o', '@OUTPUT@', '@INPUT@'])
  orc_targets += {'name': orcsrc, 'orc-source': files(orcsrc + '.orc'), 'header': orc_h, 'source': orc_c}
else
  orc_h = configure_file(input : orcsrc + '-dist.h',
    output : orcsrc + '.h',
    copy : true)
  orc_c = configure_file(input : orcsrc + '-dist.c',
    output : orcsrc + '.c',
    copy : true)
endif

gstgaudioeffects = library('gstgaudieffects',
  gaudio_sources, orc_c, orc_h,
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstvideo_dep, orc_dep, libm],
  install : true,
  install_dir : plugins_install_dir,
)
plugins += [gstgaudioeffects]
