require 'rubygems'
require 'mkmf-rice'

$CXXFLAGS << ' -std=c++11'

dir_config 'cgimap'
abort "missing standard C++ libraries" unless have_library('stdc++', 'std::string', 'string')
abort "missing cgimap headers" unless have_header('cgimap/config.hpp')
abort "missing cgimap library" unless have_library('cgimap_core', 'bbox box; box.valid()', 'cgimap/bbox.hpp')

create_makefile "cgimap/cgimap"
