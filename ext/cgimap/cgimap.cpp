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

using namespace Rice;
namespace bpo = boost::program_options;

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
