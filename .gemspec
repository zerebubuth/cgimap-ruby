# -*- Ruby -*-
Gem::Specification.new do |s|
  s.name        = 'cgimap-ruby'
  s.version     = '0.0.1'
  s.licenses    = ['GPL-2.0']
  s.summary     = "Ruby bindings for cgimap."
  s.description = "Cgimap is a library and program which partially implements the OpenStreetMap API. This gem exposes that functionality to Ruby, with the intention of being able to use it from Rails."
  s.authors     = ["Matt Amos"]
  s.email       = 'zerebubuth@gmail.com'
  s.files       = ["lib/cgimap.rb", "lib/cgimap/cgimap.so"]
  s.homepage    = 'https://github.com/zerebubuth/cgimap-ruby'

  s.add_development_dependency 'rake-compiler', '>= 0.9.4'

  # TODO - i think this is a runtime dependency... but it's possible
  # the .so doesn't actually need the rice library? check this!
  s.add_runtime_dependency 'rice'
  s.add_runtime_dependency 'rack', '>=1.6.0'
end
