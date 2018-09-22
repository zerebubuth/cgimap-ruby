require 'rack'
$:.unshift(File.expand_path('../lib'))
require 'cgimap'

class Request
  attr_reader :env
  def initialize(env)
    @env = env
  end
end

class Server
  def initialize
    @rl = Cgimap::RateLimiter.new({})
    @rt = Cgimap::Routes.new
#    @f = Cgimap::create_backend('backend' => 'staticxml', 'file' => 'empty.osm')
    @f = Cgimap::create_backend('backend'  => 'apidb', 
                                'host'     => 'localhost',
                                'cachesize' => '100000',
                                'charset'  => 'utf8',
                                'dbname'   => 'openstreetmap',
                                'username' => 'osm',
                                'password' => 'password',
                                'pidfile'  => '/dev/null',
                                'logfile'  => '/dev/stdout',
                                'memcache' => 'memcached',
                                'ratelimit' => '204800',
                                'maxdebt'   => '250' )
  end

  def call(env)
    Cgimap::process_request(Request.new(env), @rl, "Magic cgimap-ruby", @rt, @f)
  end
end

Rack::Handler::WEBrick.run(Server.new, :Port => 8000)
