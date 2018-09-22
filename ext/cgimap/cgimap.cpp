#include "rice/Module.hpp"
#include "rice/Hash.hpp"
#include "rice/Data_Type.hpp"
#include "rice/Data_Type.ipp"
#include "rice/Constructor.hpp"
#include "rice/to_from_ruby.hpp"

#include <cgimap/request.hpp>
#include <cgimap/rate_limiter.hpp>
#include <cgimap/routes.hpp>
#include <cgimap/data_selection.hpp>
#include <cgimap/backend.hpp>
#include <cgimap/output_buffer.hpp>
#include <cgimap/request_helpers.hpp>
#include <cgimap/config.hpp>
#ifdef ENABLE_APIDB
#include <cgimap/backend/apidb/apidb.hpp>
#endif
#ifdef ENABLE_PGSNAPSHOT
#include <cgimap/backend/pgsnapshot/pgsnapshot.hpp>
#endif
#include <cgimap/backend/staticxml/staticxml.hpp>
#include <cgimap/process_request.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>

using namespace Rice;
namespace bpo = boost::program_options;
namespace bal = boost::algorithm;

namespace {
bpo::variables_map convert_hash_to_vm(Hash &h) {
   bpo::variables_map vm;
   
   Hash::iterator itr = h.begin();
   const Hash::iterator end = h.end();
   for (; itr != end; ++itr) {
      int k_type = itr->first.rb_type();
      Object value(itr->second);
      int v_type = value.rb_type();
      
      if ((k_type == T_STRING) ||
          (k_type == T_SYMBOL)) {
         std::string key = itr->first.to_s().str();
         
         if (v_type == T_FIXNUM) {
            int i_value = from_ruby<int>(value);
            vm.insert(std::make_pair(key, bpo::variable_value(i_value, false)));
            
         } else {
            std::string s_value = value.to_s().str();
            vm.insert(std::make_pair(key, bpo::variable_value(s_value, false)));
         }
      }
   }

   return vm;
}

struct construct_rate_limiter {
   static void construct(Object self, Hash h) {
      bpo::variables_map vm = convert_hash_to_vm(h);

      DATA_PTR(self.value()) = new memcached_rate_limiter(vm); // new rate_limiter(vm);
   }
};

typedef boost::shared_ptr<data_selection::factory> factory_ptr_t;

factory_ptr_t create_backend_(Hash h) {
  static bool once = true;
  bpo::variables_map vm = convert_hash_to_vm(h);

  if (once) {
    once = false;

#if ENABLE_APIDB
    register_backend(make_apidb_backend());
#endif
#if ENABLE_PGSNAPSHOT
    register_backend(make_pgsnapshot_backend());
#endif
    register_backend(make_staticxml_backend());
  }

  return create_backend(vm);
}

// adapt the rack response for use by cgimap
struct rack_output_buffer : public output_buffer {
   explicit rack_output_buffer(Object io);
   virtual ~rack_output_buffer();

   virtual int write(const char *buffer, int len);
   virtual int written();
   virtual int close();
   virtual void flush();

private:
   Object m_io;
   bool m_closed;
   int m_written;
};

// implementation of output_buffer which actually buffers all
// of its output in a string.
struct string_output_buffer : public output_buffer {
  string_output_buffer();
  virtual ~string_output_buffer();

  virtual int write(const char *buffer, int len);
  virtual int written();
  virtual int close();
  virtual void flush();

  std::string value() const;

private:
  std::ostringstream m_stream;
  int m_written;
};

// encapsulate a rack request using full socket hijacking,
// and adapt it to the request interface that cgimap uses
// internally.
struct hijack_rack_request : public request {
  explicit hijack_rack_request(Object io, Hash params);
  virtual ~hijack_rack_request();

  const char *get_param(const char *key);
  void dispose();

  boost::posix_time::ptime get_current_time() const;

//  const std::string get_payload();

protected:
  void write_header_info(int status, const headers_t &headers);
  boost::shared_ptr<output_buffer> get_buffer_internal();
  void finish_internal();

private:
   boost::shared_ptr<rack_output_buffer> m_output_buffer;
   std::map<std::string, std::string> m_params;
};

// encapsulate a rack request using a buffered string response
// type, and adapt it to the request interface that cgimap uses
// internally.
struct buffered_rack_request : public request {
  explicit buffered_rack_request(Object req);
  virtual ~buffered_rack_request();

  const char *get_param(const char *key);
  void dispose();

  boost::posix_time::ptime get_current_time() const;

//  const std::string get_payload();

  Object rack_response() const;

protected:
  void write_header_info(int status, const headers_t &headers);
  boost::shared_ptr<output_buffer> get_buffer_internal();
  void finish_internal();

private:
  int m_status;
  request::headers_t m_response_headers;
  boost::shared_ptr<string_output_buffer> m_output_buffer;
  std::map<std::string, std::string> m_params;
  std::string m_request_uri;
};

rack_output_buffer::rack_output_buffer(Object io)
  : m_io(io)
  , m_closed(false)
  , m_written(0) {
}

rack_output_buffer::~rack_output_buffer() {
   if (!m_closed) {
      try {
         close();
      } catch (...) {
         // can't throw in destructor anyway...
         // TODO: log this.
      }
   }
}

int rack_output_buffer::write(const char *buffer, int len) {
   if (m_closed) {
      throw std::runtime_error("Attempt to write to a closed output buffer.");
   }

   // ugh... nasty hack to pull out the header/status line first.
   // TODO: convert cgimap to return status/headers first without
   // stringifying them, then write to an IO for the body.
   if (m_written == 0) {
     if (len < 8) { throw std::runtime_error("First write too short."); }
     if (strncmp(buffer, "Status: ", 8) != 0) {
       throw std::runtime_error("First line should be status.");
     }
     char *end = (char *)memmem(buffer, len, "\r\n", 2);
     if (end == NULL) { throw std::runtime_error("First line too short."); }
     int written = from_ruby<int>(m_io.call<String>("write", "HTTP/1.1 "));
     m_written += written;
     if (written < 9) { throw std::runtime_error("First write too short."); }

     // adjust buffer to account for what we already wrote
     buffer += 8;
     len -= 8;
   }

   String s(std::string(buffer, len));
   int written = from_ruby<int>(m_io.call<String>("write", s));
   m_written += written;
   return written;
}

int rack_output_buffer::written() {
   return m_written;
}

int rack_output_buffer::close() {
   m_io.call("close");
   m_closed = true;
   return 0;
}

void rack_output_buffer::flush() {
  m_io.call("flush");
}

string_output_buffer::string_output_buffer()
  : m_stream()
  , m_written(0) {
}

string_output_buffer::~string_output_buffer() {
}

int string_output_buffer::write(const char *buffer, int len) {
  m_stream.write(buffer, len);
  return m_stream.good() ? len : -1;
}

int string_output_buffer::written() {
  return m_stream.tellp();
}

int string_output_buffer::close() {
  return 0;
}

void string_output_buffer::flush() {
}

std::string string_output_buffer::value() const {
  return m_stream.str();
}

std::map<std::string, std::string> hash_to_stdmap(Hash params) {
   std::map<std::string, std::string> c_params;

   // note that we copy the environment into a C++ structure here
   // because we need to control the lifetime. the interface to
   // cgimap returns a raw pointer (TODO: it shouldn't), and
   // therefore we have to assume that the pointer must remain
   // valid for the duration of the request, which we can't
   // guarantee if Ruby's GC moves the string.
   Hash::iterator itr = params.begin();
   const Hash::iterator end = params.end();
   for (; itr != end; ++itr) {
      std::string key = itr->first.to_s().str();
      // Rack uppercases all the 'CGI' variables, and we're not
      // interested in anything that's an internal Rack variable,
      // as cgimap would never understand it anyway.
      if (bal::all(key, bal::is_from_range('A', 'Z') || bal::is_from_range('_', '_'))) {
         c_params.insert(std::make_pair(key, Object(itr->second).to_s().str()));
      }
   }

   return c_params;
}

Hash headers_to_hash(const std::vector<std::pair<std::string, std::string> > &headers) {
  typedef std::pair<std::string, std::string> pair_t;

  Hash h;

  BOOST_FOREACH(const pair_t &val, headers) {
    h[String(val.first)] = String(val.second);
  }

  return h;
}

const char *get_param_stdmap(const std::map<std::string, std::string> &params,
                             const char *key) {
   const std::string k(key);
   std::map<std::string, std::string>::const_iterator itr = params.find(k);

   if (itr == params.end()) {
      return NULL;

   } else {
      return itr->second.c_str();
   }
}

hijack_rack_request::hijack_rack_request(Object io, Hash params)
  : m_output_buffer(new rack_output_buffer(io))
  , m_params(hash_to_stdmap(params)) {
}

hijack_rack_request::~hijack_rack_request() {
}

const char *hijack_rack_request::get_param(const char *key) {
  return get_param_stdmap(m_params, key);
}

void hijack_rack_request::dispose() {
}

void hijack_rack_request::write_header_info(int status, const headers_t &headers) {
  std::ostringstream ostr;
  ostr << "Status: " << status << " " << status_message(status) << "\r\n";
  BOOST_FOREACH(const request::headers_t::value_type &header, headers) {
    ostr << header.first << ": " << header.second << "\r\n";
  }
  ostr << "\r\n";
  std::string data(ostr.str());
  m_output_buffer->write(&data[0], data.size());
}

boost::shared_ptr<output_buffer> hijack_rack_request::get_buffer_internal() {
   return m_output_buffer;
}

void hijack_rack_request::finish_internal() {
}


boost::posix_time::ptime hijack_rack_request::get_current_time() const {

  return boost::posix_time::second_clock::local_time();
}

/*

const std::string hijack_rack_request::get_payload() {
  return "";
}

*/

buffered_rack_request::buffered_rack_request(Object req)
  : m_status(-1)
  , m_response_headers()
  , m_output_buffer(new string_output_buffer)
  , m_params(hash_to_stdmap(Hash(req.call("env"))))
  , m_request_uri() {

  // figure out the full path: this seems to be the concatenation of
  // script name and path info. it's arguable whether this should be
  // the case, and perhaps we should only parse the path_info
  // part... well, we can sort that out later.
  std::string path = (boost::format("%1%%2%")
                      % m_params["SCRIPT_NAME"]
                      % m_params["PATH_INFO"]).str();

  std::map<std::string, std::string>::iterator query_itr = m_params.find("QUERY_STRING");
  if ((query_itr == m_params.end()) || (query_itr->second.empty())) {
    m_request_uri = path;

  } else {
    m_request_uri = (boost::format("%1%?%2%") % path % query_itr->second).str();
  }
}

buffered_rack_request::~buffered_rack_request() {
}

const char *buffered_rack_request::get_param(const char *key) {
  // hack for request_uri - not sure if we're using it wrong
  // or if the value is different between FCGI and Rack.
  if (strncmp(key, "REQUEST_URI", 12) == 0) {
    return m_request_uri.c_str();

  } else {
    return get_param_stdmap(m_params, key);
  }
}

void buffered_rack_request::dispose() {
}

Object buffered_rack_request::rack_response() const {
  Array response;

  response.push(m_status);
  response.push(headers_to_hash(m_response_headers));
  Array body;
  body.push(String(m_output_buffer->value()));
  response.push(body);

  return response;
}

void buffered_rack_request::write_header_info(int status, const request::headers_t &headers) {
  m_status = status;
  m_response_headers = headers;
}

boost::shared_ptr<output_buffer> buffered_rack_request::get_buffer_internal() {
  return m_output_buffer;
}

void buffered_rack_request::finish_internal() {
}


boost::posix_time::ptime buffered_rack_request::get_current_time() const {

  return boost::posix_time::second_clock::local_time();
}

/*
const std::string buffered_rack_request::get_payload() {
  return "";
}
*/

Object process_request_(Object r_req, rate_limiter &rl, const std::string &generator,
                      routes &r, factory_ptr_t factory) {
  Hash env(r_req.call("env"));

  // env['rack.hijack'].call starts the hijacking
  Object hijack = env[String("rack.hijack")];

  if (!hijack.is_nil()) {
    // perform the hijack and grab the socket to use later.
    Object io;
    try {
      hijack.call("call");
      io = env[String("rack.hijack_io")];

    } catch (const Rice::Exception_Base &e) {
      if (e.class_of().to_s().str() != std::string("NotImplementedError")) {
        throw;
      }
    }

    if (!io.is_nil()) {
      hijack_rack_request req(io, env);
      process_request(req, rl, generator, r, factory, nullptr);
      return Object();
    }
  }

  buffered_rack_request req(r_req);
  process_request(req, rl, generator, r, factory, nullptr);
  return req.rack_response();
}

} // anonymous namespace

extern "C" void Init_cgimap() {
   Module rb_mCgimap = define_module("Cgimap");

   Data_Type<rate_limiter> rb_cRateLimiter =
      define_class_under<rate_limiter>(rb_mCgimap, "RateLimiter")
      .define_constructor(construct_rate_limiter())
      .define_method("check", &rate_limiter::check)
      .define_method("update", &rate_limiter::update)
      ;

   Data_Type<routes> rb_cRoutes =
      define_class_under<routes>(rb_mCgimap, "Routes")
      .define_constructor(Constructor<routes>())
      ;

   Data_Type<factory_ptr_t> rb_cFactory =
      define_class_under<factory_ptr_t>(rb_mCgimap, "Factory")
      ;

   rb_mCgimap.define_singleton_method("create_backend", &create_backend_);
   rb_mCgimap.define_singleton_method("process_request", &process_request_);
}
