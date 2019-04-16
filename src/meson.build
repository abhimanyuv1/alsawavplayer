wavplayer_sources = [
  'main.c',
]

cc = meson.get_compiler('c')
asound_dep = cc.find_library('asound', required : true)

wavplayer_deps = [
  asound_dep,
]

executable('wavplayer', wavplayer_sources,
  dependencies: wavplayer_deps,
  install: true,
)
