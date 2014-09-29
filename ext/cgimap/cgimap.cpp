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

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/classification.hpp>

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

      DATA_PTR(self.value()) = new rate_limiter(vm);
   }
};

typedef boost::shared_ptr<data_selection::factory> factory_ptr_t;

factory_ptr_t create_backend_(Hash h) {
   bpo::variables_map vm = convert_hash_to_vm(h);

   return create_backend(vm);
}

// adapt the rack response for use by cgimap
struct rack_output_buffer : public output_buffer {
   explicit rack_output_buffer(Hash env);
   virtual ~rack_output_buffer();

   virtual int write(const char *buffer, int len);
   virtual int written();
   virtual int close();
   virtual void flush();

private:
   bool m_closed;
   int m_written;
   Object m_io;
};

// encapsulate a rack request, as used (?) internally by rails
// and adapt it to the request interface that cgimap uses
// internally.
struct rack_request : public request {
   explicit rack_request(Object req);
   virtual ~rack_request();

   virtual const char *get_param(const char *key);
   virtual boost::shared_ptr<output_buffer> get_buffer();
   virtual std::string extra_headers();
   virtual void finish();

private:
   boost::shared_ptr<rack_output_buffer> m_output_buffer;
   std::map<std::string, std::string> m_params;
};

rack_output_buffer::rack_output_buffer(Hash env)
   : m_closed(false)
   , m_written(0) {
   // env['rack.hijack'].call starts the hijacking
   Object hijack = env[String("rack.hijack")];

   if (hijack.is_nil()) {
      // the Rack we're running in doesn't support hijacking
      throw std::runtime_error("The version of Rack in use does not support "
                               "socket hijacking, which cgimap-ruby requires.");
   }

   // perform the hijack and grab the socket to use later.
   hijack.call("call");
   m_io = env[String("rack.hijack_io")];

   if (m_io.is_nil()) {
      // something went wrong with the hijack :-(
      throw std::runtime_error("Unable to hijack Rack socket. Cgimap-ruby "
                               "won't work.");
   }
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

   // try to avoid copying as much as possible...
   Builtin_Object<RString, T_STRING> rb_buf(protect(rb_str_new, buffer, len));
   int written = m_io.call<int>("write", rb_buf);
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

rack_request::rack_request(Object req)
   : m_output_buffer(new rack_output_buffer(Hash(req.attr_get("env"))))
   , m_params() {

   Hash params(req.attr_get("env"));

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
         m_params.insert(std::make_pair(key, Object(itr->second).to_s().str()));
      }
   }
}

rack_request::~rack_request() {
}

const char *rack_request::get_param(const char *key) {
   const std::string k(key);
   std::map<std::string, std::string>::iterator itr = m_params.find(k);

   if (itr == m_params.end()) {
      return NULL;

   } else {
      return itr->second.c_str();
   }
}

boost::shared_ptr<output_buffer> rack_request::get_buffer() {
   return m_output_buffer;
}

std::string rack_request::extra_headers() {
   // because we're hijacking the Rack socket, we take over complete
   // responsibility for the socket from that point onwards. this
   // also means Keep-Alive. since re-implementing Keep-Alive is
   // more than we really want to do, instead we inject a header to
   // tell the client to close the connection.

   // this really isn't ideal, and would be much better to perform
   // a 'partial' hijacking, but this requires extensive changes to
   // how cgimap generates its response and will have to remain a
   // TODO until someone tackles it.

   return "Connection: close\r\n";
}

void rack_request::finish() {
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
}
