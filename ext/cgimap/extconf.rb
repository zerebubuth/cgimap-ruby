require 'rubygems'
require 'mkmf-rice'

$CXXFLAGS << ' -std=c++11 -DHAVE_CXX11'
$CPPFLAGS << ' -I/usr/include/libxml2'

dir_config 'cgimap'
abort "missing standard C++ libraries" unless have_library('stdc++', 'std::string s("")', 'string')
abort "missing cgimap headers" unless have_header('cgimap/config.hpp')
abort "missing pqxx" unless have_library('pqxx', 'pqxx::connection c("")', 'pqxx/pqxx')
abort "missing boost::program_options" unless have_library('boost_program_options', 'boost::program_options::options_description options("")', 'boost/program_options.hpp')
abort "missing boost::regex" unless have_library('boost_regex', 'boost::regex re("")', 'boost/regex.hpp')
abort "missing boost::locale" unless have_library('boost_locale', 'std::string demo = boost::locale::to_upper("demo")', 'boost/locale.hpp')
abort "missing boost::system" unless have_library('boost_system', 'boost::system::error_code ec = boost::system::errc::make_error_code(boost::system::errc::not_supported)','boost/system/error_code.hpp')
abort "missing boost::date_time" unless have_library('boost_date_time', 'boost::gregorian::date d(2002,boost::gregorian::Jan,10)','boost/date_time/gregorian/gregorian.hpp')
abort "missing libxml2" unless have_library('xml2', 'xmlTextWriterPtr ptr = xmlNewTextWriter(NULL)', 'libxml/xmlwriter.h')
abort "missing zlib" unless have_library('z', 'z_stream strm;inflateInit(&strm)', 'zlib.h')
abort "missing crypto++" unless have_library('crypto++', 'CryptoPP::Exception * e = new CryptoPP::Exception(CryptoPP::Exception::NOT_IMPLEMENTED, "test")','crypto++/cryptlib.h')
abort "missing cgimap core library" unless have_library('cgimap_core', 'bbox box; box.valid()', 'cgimap/bbox.hpp')
abort "missing cgimap apidb library" unless have_library('cgimap_apidb', 'make_apidb_backend()', 'cgimap/backend/apidb/apidb.hpp')
abort "missing cgimap pgsnapshot library" unless have_library('cgimap_pgsnapshot', 'make_pgsnapshot_backend()', 'cgimap/backend/pgsnapshot/pgsnapshot.hpp')
abort "missing cgimap staticxml library" unless have_library('cgimap_staticxml', 'make_staticxml_backend()', 'cgimap/backend/staticxml/staticxml.hpp')

create_makefile "cgimap/cgimap"
