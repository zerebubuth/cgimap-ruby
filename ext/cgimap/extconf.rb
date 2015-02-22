require 'rubygems'
require 'mkmf-rice'

$CXXFLAGS << ' -std=c++11'
$CPPFLAGS << ' -I/usr/include/libxml2'

dir_config 'cgimap'
abort "missing standard C++ libraries" unless have_library('stdc++', 'std::string', 'string')
abort "missing cgimap headers" unless have_header('cgimap/config.hpp')
abort "missing pqxx" unless have_library('pqxx', 'pqxx::connection c', 'pqxx/pqxx')
abort "missing boost::program_options" unless have_library('boost_program_options', 'boost::program_options::options_description options("")', 'boost/program_options.hpp')
abort "missing boost::regex" unless have_library('boost_regex', 'boost::regex re("")', 'boost/regex.hpp')
abort "missing libxml2" unless have_library('xml2', 'xmlTextWriterPtr ptr = xmlNewTextWriter(NULL)', 'libxml/xmlwriter.h')
abort "missing zlib" unless have_library('z', 'z_stream z', 'zlib.h')
abort "missing cgimap core library" unless have_library('cgimap_core', 'bbox box; box.valid()', 'cgimap/bbox.hpp')
abort "missing cgimap apidb library" unless have_library('cgimap_apidb', 'make_apidb_backend()', 'cgimap/backend/apidb/apidb.hpp')
abort "missing cgimap pgsnapshot library" unless have_library('cgimap_pgsnapshot', 'make_pgsnapshot_backend()', 'cgimap/backend/pgsnapshot/pgsnapshot.hpp')
abort "missing cgimap staticxml library" unless have_library('cgimap_staticxml', 'make_staticxml_backend()', 'cgimap/backend/staticxml/staticxml.hpp')

create_makefile "cgimap/cgimap"
