require 'rubygems'
require 'mkmf-rice'

$CXXFLAGS << ' -std=c++11'

dir_config 'cgimap'
abort "missing standard C++ libraries" unless have_library('stdc++', 'std::string', 'string')
abort "missing cgimap headers" unless have_header('cgimap/config.hpp')
abort "missing cgimap core library" unless have_library('cgimap_core', 'bbox box; box.valid()', 'cgimap/bbox.hpp')
abort "missing cgimap apidb library" unless have_library('cgimap_apidb', 'make_apidb_backend()', 'cgimap/backend/apidb/apidb.hpp')
abort "missing cgimap pgsnapshot library" unless have_library('cgimap_pgsnapshot', 'make_pgsnapshot_backend()', 'cgimap/backend/pgsnapshot/pgsnapshot.hpp')
abort "missing cgimap staticxml library" unless have_library('cgimap_staticxml', 'make_staticxml_backend()', 'cgimap/backend/staticxml/staticxml.hpp')

create_makefile "cgimap/cgimap"
